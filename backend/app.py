import asyncio
import base64
import json
import logging
import secrets
import uuid
from contextlib import asynccontextmanager
from typing import Annotated
from urllib.parse import quote

from fastapi import (
    Depends,
    FastAPI,
    File,
    Form,
    Header,
    HTTPException,
    UploadFile,
    WebSocket,
    WebSocketDisconnect,
)
from fastapi.responses import Response
from openai import APIError, APITimeoutError

from backend.audio import read_wav_upload
from backend.config import Settings, get_settings
from backend.conversations import ConversationStore
from backend.live_providers import LiveProvider, build_live_providers
from backend.providers import VoiceProvider, create_provider
from backend.schemas import ChatRequest, ChatResponse, SpeechRequest, VoiceResponse

logger = logging.getLogger(__name__)


def create_app(
    settings: Settings | None = None,
    provider: VoiceProvider | None = None,
    live_providers: dict[str, LiveProvider] | None = None,
) -> FastAPI:
    config = settings or get_settings()

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.store = ConversationStore(config.database_path)
        try:
            app.state.provider = provider or create_provider(config)
            app.state.live_providers = (
                live_providers if live_providers is not None else build_live_providers(config)
            )
        except RuntimeError as exc:
            raise RuntimeError(str(exc)) from exc
        yield

    api = FastAPI(title="Voice Interactive Robot API", version="0.1.0", lifespan=lifespan)

    async def authorize(authorization: str | None = Header(default=None)) -> None:
        if not config.device_auth_token:
            return
        supplied = authorization.removeprefix("Bearer ") if authorization else ""
        if not secrets.compare_digest(supplied, config.device_auth_token):
            raise HTTPException(status_code=401, detail="Invalid device token")

    async def call_provider(coro):
        try:
            return await coro
        except APITimeoutError as exc:
            raise HTTPException(status_code=504, detail="AI provider timed out") from exc
        except APIError as exc:
            raise HTTPException(status_code=502, detail="AI provider request failed") from exc

    @api.get("/health")
    async def health():
        available = [name for name, item in api.state.live_providers.items() if item.configured()]
        return {
            "status": "ok",
            "mode": config.ai_mode,
            "live_providers_configured": available,
            "audio": {"input_rate": 16000, "output_rate": 24000, "format": "s16le/mono"},
        }

    @api.websocket("/v1/voice/live")
    async def live_voice(websocket: WebSocket):
        if config.device_auth_token:
            supplied = websocket.headers.get("authorization", "")
            token = supplied.removeprefix("Bearer ")
            if not secrets.compare_digest(token, config.device_auth_token):
                await websocket.close(code=4401, reason="Invalid device token")
                return
        await websocket.accept()
        requested = websocket.query_params.get("provider", "auto").lower()
        if requested == "auto":
            names = [name.strip() for name in config.live_provider_order.split(",") if name.strip()]
            if config.ai_mode == "mock" and "mock_live" not in names:
                names.append("mock_live")
        else:
            names = [requested]
        session = None
        selected_name = ""
        failures: list[str] = []
        for name in names:
            candidate = api.state.live_providers.get(name)
            if candidate is None or not candidate.configured():
                failures.append(f"{name}:not_configured")
                continue
            try:
                await websocket.send_json({"type": "connecting", "provider": name})
                session = await asyncio.wait_for(
                    candidate.connect(config.system_prompt),
                    timeout=config.live_session_timeout_seconds,
                )
                selected_name = name
                break
            except Exception as exc:  # noqa: BLE001 - vendor SDK errors differ by provider
                failures.append(f"{name}:{type(exc).__name__}")
                logger.warning("Live provider %s failed to start: %s", name, type(exc).__name__)
        if session is None:
            await websocket.send_json({"type": "error", "code": "no_live_provider", "providers": failures})
            await websocket.close(code=1013, reason="No configured live provider accepted the session")
            return

        await websocket.send_json(
            {"type": "session_started", "provider": selected_name, "input_rate": 16000, "output_rate": 24000}
        )

        async def send_input() -> None:
            while True:
                packet = await websocket.receive()
                if packet["type"] == "websocket.disconnect":
                    return
                text = packet.get("text")
                if text is not None:
                    try:
                        control = json.loads(text)
                    except json.JSONDecodeError:
                        await websocket.send_json({"type": "error", "code": "invalid_control_json"})
                        continue
                    if control.get("type") == "interrupt":
                        await session.interrupt()
                    elif control.get("type") == "close":
                        return
                    elif control.get("type") != "start":
                        await websocket.send_json({"type": "error", "code": "unknown_control"})
                    continue
                audio = packet.get("bytes")
                if audio:
                    if len(audio) > 8192 or len(audio) % 2:
                        await websocket.send_json({"type": "error", "code": "invalid_audio_frame"})
                        continue
                    await session.send_audio(audio)

        async def send_output() -> None:
            async for event in session.receive():
                if event.kind == "audio":
                    if len(event.data) % 2:
                        continue
                    await websocket.send_bytes(event.data)
                elif event.text:
                    await websocket.send_text(event.text)

        tasks = [asyncio.create_task(send_input()), asyncio.create_task(send_output())]
        try:
            done, _pending = await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
            for task in done:
                task.result()
        except WebSocketDisconnect:
            pass
        except Exception as exc:  # noqa: BLE001 - provider and socket failures are heterogeneous
            logger.info("Live session ended (%s)", type(exc).__name__)
            try:
                await websocket.send_json({"type": "error", "code": "live_session_failed"})
            except (RuntimeError, WebSocketDisconnect) as send_exc:
                logger.debug("Could not send live session error (%s)", type(send_exc).__name__)
        finally:
            for task in tasks:
                if not task.done():
                    task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            await session.close()

    @api.post("/v1/transcribe", dependencies=[Depends(authorize)])
    async def transcribe(file: Annotated[UploadFile, File()]):
        wav_data = await read_wav_upload(file, config.max_audio_bytes)
        text = await call_provider(api.state.provider.transcribe(wav_data))
        return {"transcript": text}

    async def chat_text(payload: ChatRequest) -> ChatResponse:
        store: ConversationStore = api.state.store
        store.add(payload.conversation_id, "user", payload.text)
        history = store.history(payload.conversation_id)
        messages = [{"role": "system", "content": config.system_prompt}, *history]
        answer = await call_provider(api.state.provider.reply(messages))
        store.add(payload.conversation_id, "assistant", answer)
        return ChatResponse(conversation_id=payload.conversation_id, transcript=payload.text, response_text=answer)

    @api.post("/v1/chat", response_model=ChatResponse, dependencies=[Depends(authorize)])
    async def chat(payload: ChatRequest):
        return await chat_text(payload)

    @api.post("/v1/speech", dependencies=[Depends(authorize)])
    async def speech(payload: SpeechRequest):
        audio = await call_provider(api.state.provider.speak(payload.text))
        return Response(
            content=audio.pcm,
            media_type="application/octet-stream",
            headers={"X-Audio-Format": audio.sample_format, "X-Sample-Rate": str(audio.sample_rate), "X-Channels": str(audio.channels)},
        )

    @api.post("/v1/voice", response_model=VoiceResponse, dependencies=[Depends(authorize)])
    async def voice(
        file: Annotated[UploadFile, File()],
        conversation_id: Annotated[str | None, Form()] = None,
        device_id: Annotated[str, Form()] = "t4-s3",
    ):
        wav_data = await read_wav_upload(file, config.max_audio_bytes)
        conversation_id = conversation_id or f"{device_id}-{uuid.uuid4().hex[:12]}"
        transcript = await call_provider(api.state.provider.transcribe(wav_data))
        if not transcript:
            raise HTTPException(status_code=422, detail="Speech could not be transcribed")
        result = await chat_text(ChatRequest(text=transcript, conversation_id=conversation_id, device_id=device_id))
        audio = await call_provider(api.state.provider.speak(result.response_text))
        return VoiceResponse(
            conversation_id=conversation_id,
            transcript=transcript,
            response_text=result.response_text,
            audio=base64.b64encode(audio.pcm).decode("ascii"),
            audio_format=audio.sample_format,
            sample_rate=audio.sample_rate,
            channels=audio.channels,
            audio_bytes=len(audio.pcm),
        )

    @api.post("/v1/voice/audio", dependencies=[Depends(authorize)])
    async def voice_audio(
        file: Annotated[UploadFile, File()],
        conversation_id: Annotated[str | None, Form()] = None,
        device_id: Annotated[str, Form()] = "t4-s3",
    ):
        """Robot-friendly equivalent of /v1/voice, returning raw PCM for streaming playback."""
        wav_data = await read_wav_upload(file, config.max_audio_bytes)
        conversation_id = conversation_id or f"{device_id}-{uuid.uuid4().hex[:12]}"
        transcript = await call_provider(api.state.provider.transcribe(wav_data))
        if not transcript:
            raise HTTPException(status_code=422, detail="Speech could not be transcribed")
        result = await chat_text(ChatRequest(text=transcript, conversation_id=conversation_id, device_id=device_id))
        audio = await call_provider(api.state.provider.speak(result.response_text))
        return Response(
            content=audio.pcm,
            media_type="application/octet-stream",
            headers={
                "X-Conversation-Id": conversation_id,
                "X-Transcript": quote(result.transcript, safe=""),
                "X-Response-Text": quote(result.response_text, safe=""),
                "X-Audio-Format": audio.sample_format,
                "X-Sample-Rate": str(audio.sample_rate),
                "X-Channels": str(audio.channels),
            },
        )

    return api


app = create_app()

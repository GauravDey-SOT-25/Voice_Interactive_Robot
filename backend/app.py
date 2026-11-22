import base64
import secrets
import uuid
from contextlib import asynccontextmanager
from typing import Annotated
from urllib.parse import quote

from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, UploadFile
from fastapi.responses import Response
from openai import APIError, APITimeoutError

from backend.audio import read_wav_upload
from backend.config import Settings, get_settings
from backend.conversations import ConversationStore
from backend.providers import VoiceProvider, create_provider
from backend.schemas import ChatRequest, ChatResponse, SpeechRequest, VoiceResponse


def create_app(settings: Settings | None = None, provider: VoiceProvider | None = None) -> FastAPI:
    config = settings or get_settings()

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.store = ConversationStore(config.database_path)
        try:
            app.state.provider = provider or create_provider(config)
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
        return {"status": "ok", "mode": config.ai_mode, "audio": {"input_rate": 16000, "output_rate": 24000}}

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

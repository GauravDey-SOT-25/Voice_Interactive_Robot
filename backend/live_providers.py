"""Streaming speech-to-speech provider adapters used by the robot WebSocket gateway."""

from __future__ import annotations

import asyncio
import base64
import json
import logging
from collections.abc import AsyncIterator
from dataclasses import dataclass
from typing import Any, Protocol
from urllib.parse import quote

import websockets
from websockets.asyncio.client import ClientConnection

from backend.config import Settings

logger = logging.getLogger(__name__)


@dataclass(frozen=True)
class LiveEvent:
    kind: str
    data: bytes = b""
    text: str = ""


class LiveSession(Protocol):
    provider: str

    async def send_audio(self, pcm16_16khz: bytes) -> None: ...
    async def interrupt(self) -> None: ...
    def receive(self) -> AsyncIterator[LiveEvent]: ...
    async def close(self) -> None: ...


class LiveProvider(Protocol):
    name: str

    def configured(self) -> bool: ...
    async def connect(self, instructions: str) -> LiveSession: ...


def resample_pcm16(pcm: bytes, source_rate: int, target_rate: int) -> bytes:
    """Small, dependency-free mono PCM16 linear resampler for short live frames."""
    import array

    if source_rate == target_rate or not pcm:
        return pcm
    if len(pcm) % 2:
        pcm = pcm[:-1]
    source = array.array("h")
    source.frombytes(pcm)
    if source_rate <= 0 or target_rate <= 0 or not source:
        return b""
    output_count = max(1, round(len(source) * target_rate / source_rate))
    output = array.array("h")
    scale = source_rate / target_rate
    for index in range(output_count):
        position = index * scale
        left = min(int(position), len(source) - 1)
        right = min(left + 1, len(source) - 1)
        fraction = position - left
        value = round(source[left] * (1 - fraction) + source[right] * fraction)
        output.append(max(-32768, min(32767, value)))
    return output.tobytes()


async def _json_event(socket: ClientConnection, event_type: str, **payload: Any) -> None:
    await socket.send(json.dumps({"type": event_type, **payload}, separators=(",", ":")))


async def _receive_json(socket: ClientConnection, timeout: float) -> dict[str, Any]:
    raw = await asyncio.wait_for(socket.recv(), timeout=timeout)
    if not isinstance(raw, str):
        raise TypeError("Provider sent an unexpected binary control event")
    event = json.loads(raw)
    if event.get("type") == "error":
        raise RuntimeError(f"Provider rejected live session: {event.get('error', event)}")
    return event


class OpenAIRealtimeSession:
    provider = "openai_realtime"

    def __init__(self, socket: ClientConnection):
        self.socket = socket

    async def send_audio(self, pcm16_16khz: bytes) -> None:
        pcm24 = resample_pcm16(pcm16_16khz, 16_000, 24_000)
        await _json_event(
            self.socket,
            "input_audio_buffer.append",
            audio=base64.b64encode(pcm24).decode("ascii"),
        )

    async def interrupt(self) -> None:
        await _json_event(self.socket, "response.cancel")

    async def receive(self) -> AsyncIterator[LiveEvent]:
        async for raw in self.socket:
            if not isinstance(raw, str):
                continue
            event = json.loads(raw)
            kind = event.get("type", "")
            if kind in {"response.output_audio.delta", "response.audio.delta"}:
                audio = base64.b64decode(event.get("delta", ""))
                if audio:
                    yield LiveEvent("audio", data=audio)
            elif kind in {
                "conversation.item.input_audio_transcription.completed",
                "response.output_audio_transcript.delta",
                "response.output_audio_transcript.done",
                "error",
                "input_audio_buffer.speech_started",
            }:
                yield LiveEvent("event", text=json.dumps(event, separators=(",", ":")))
            elif kind == "response.done":
                yield LiveEvent("event", text='{"type":"response_complete"}')
            elif kind == "session.closed":
                return

    async def close(self) -> None:
        await self.socket.close()


class OpenAIRealtimeProvider:
    name = "openai_realtime"

    def __init__(self, settings: Settings):
        self.settings = settings

    def configured(self) -> bool:
        return bool(self.settings.openai_api_key)

    async def connect(self, instructions: str) -> OpenAIRealtimeSession:
        socket = await websockets.connect(
            f"wss://api.openai.com/v1/realtime?model={quote(self.settings.openai_realtime_model)}",
            additional_headers={
                "Authorization": f"Bearer {self.settings.openai_api_key}",
                "OpenAI-Safety-Identifier": "t4-s3-robot",
            },
            open_timeout=self.settings.live_session_timeout_seconds,
            ping_interval=20,
            max_size=None,
        )
        session = OpenAIRealtimeSession(socket)
        try:
            # Consume the initial session.created event before applying session configuration.
            created = await _receive_json(socket, self.settings.live_session_timeout_seconds)
            if created.get("type") not in {"session.created", "session.updated"}:
                raise RuntimeError(f"Unexpected Realtime startup event: {created.get('type')}")
            await _json_event(
                socket,
                "session.update",
                session={
                    "type": "realtime",
                    "model": self.settings.openai_realtime_model,
                    "output_modalities": ["audio"],
                    "instructions": instructions,
                    "audio": {
                        "input": {
                            "format": {"type": "audio/pcm", "rate": 24_000},
                            "turn_detection": {
                                "type": "server_vad",
                                "threshold": self.settings.live_vad_threshold,
                                "prefix_padding_ms": self.settings.live_prefix_padding_ms,
                                "silence_duration_ms": self.settings.live_silence_duration_ms,
                                "interrupt_response": True,
                                "create_response": True,
                            },
                        },
                        "output": {
                            "format": {"type": "audio/pcm", "rate": 24_000},
                            "voice": self.settings.openai_realtime_voice,
                        },
                    },
                },
            )
            while True:
                updated = await _receive_json(socket, self.settings.live_session_timeout_seconds)
                if updated.get("type") == "session.updated":
                    return session
        except Exception:
            await socket.close()
            raise


class GeminiLiveSession:
    provider = "gemini_live"

    def __init__(self, context: Any, session: Any):
        self.context = context
        self.session = session

    async def send_audio(self, pcm16_16khz: bytes) -> None:
        from google.genai import types

        await self.session.send_realtime_input(
            audio=types.Blob(data=pcm16_16khz, mime_type="audio/pcm;rate=16000")
        )

    async def interrupt(self) -> None:
        # Gemini detects the new user speech from the still-open microphone stream.
        return

    async def receive(self) -> AsyncIterator[LiveEvent]:
        async for message in self.session.receive():
            server_content = getattr(message, "server_content", None)
            if server_content is None:
                continue
            turn = getattr(server_content, "model_turn", None)
            for part in getattr(turn, "parts", []) or []:
                inline = getattr(part, "inline_data", None)
                data = getattr(inline, "data", None) if inline else None
                if data:
                    yield LiveEvent("audio", data=bytes(data))
                text = getattr(part, "text", None)
                if text:
                    yield LiveEvent("event", text=json.dumps({"type": "transcript", "text": text}))
            transcript = getattr(server_content, "input_transcription", None)
            transcript_text = getattr(transcript, "text", None) if transcript else None
            if transcript_text:
                yield LiveEvent(
                    "event", text=json.dumps({"type": "input_transcript", "text": transcript_text})
                )
            if getattr(server_content, "turn_complete", False):
                yield LiveEvent("event", text='{"type":"response_complete"}')

    async def close(self) -> None:
        await self.context.__aexit__(None, None, None)


class GeminiLiveProvider:
    name = "gemini_live"

    def __init__(self, settings: Settings):
        self.settings = settings

    def configured(self) -> bool:
        return bool(self.settings.google_api_key)

    async def connect(self, instructions: str) -> GeminiLiveSession:
        if not self.configured():
            raise RuntimeError("GOOGLE_API_KEY is not configured")
        from google import genai
        from google.genai import types

        client = genai.Client(api_key=self.settings.google_api_key)
        config = types.LiveConnectConfig(
            response_modalities=["AUDIO"],
            system_instruction=instructions,
        )
        context = client.aio.live.connect(model=self.settings.gemini_live_model, config=config)
        session = await asyncio.wait_for(
            context.__aenter__(), timeout=self.settings.live_session_timeout_seconds
        )
        return GeminiLiveSession(context, session)


class MockLiveSession:
    """Deterministic local stream fixture; emits a short tone after mic PCM arrives."""

    provider = "mock_live"

    def __init__(self):
        self.events: asyncio.Queue[LiveEvent | None] = asyncio.Queue()
        self.replied = False

    async def send_audio(self, pcm16_16khz: bytes) -> None:
        if not pcm16_16khz or self.replied:
            return
        self.replied = True
        import array
        import math

        tone = array.array(
            "h",
            (int(3500 * math.sin(2 * math.pi * 660 * i / 24_000)) for i in range(2400)),
        )
        await self.events.put(LiveEvent("event", text='{"type":"mock_response","text":"local stream test"}'))
        await self.events.put(LiveEvent("audio", data=tone.tobytes()))
        await self.events.put(LiveEvent("event", text='{"type":"response_complete"}'))

    async def interrupt(self) -> None:
        await self.events.put(LiveEvent("event", text='{"type":"interrupted"}'))

    async def receive(self) -> AsyncIterator[LiveEvent]:
        while (event := await self.events.get()) is not None:
            yield event

    async def close(self) -> None:
        await self.events.put(None)


class MockLiveProvider:
    name = "mock_live"

    def __init__(self, settings: Settings):
        self.settings = settings

    def configured(self) -> bool:
        return self.settings.ai_mode == "mock"

    async def connect(self, instructions: str) -> MockLiveSession:
        del instructions
        return MockLiveSession()


def build_live_providers(settings: Settings) -> dict[str, LiveProvider]:
    providers: dict[str, LiveProvider] = {
        "openai_realtime": OpenAIRealtimeProvider(settings),
        "gemini_live": GeminiLiveProvider(settings),
        "mock_live": MockLiveProvider(settings),
    }
    return providers

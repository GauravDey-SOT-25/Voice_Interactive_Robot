import math
import struct
from dataclasses import dataclass
from typing import Protocol

from openai import AsyncOpenAI

from backend.config import Settings


@dataclass(frozen=True)
class SpeechResult:
    pcm: bytes
    sample_rate: int = 24_000
    channels: int = 1
    sample_format: str = "s16le"


class VoiceProvider(Protocol):
    async def transcribe(self, wav_data: bytes) -> str: ...
    async def reply(self, messages: list[dict[str, str]]) -> str: ...
    async def speak(self, text: str) -> SpeechResult: ...


class MockProvider:
    """Deterministic, offline provider used for setup and integration tests."""

    async def transcribe(self, wav_data: bytes) -> str:
        return "Hello robot"

    async def reply(self, messages: list[dict[str, str]]) -> str:
        latest = next((m["content"] for m in reversed(messages) if m["role"] == "user"), "")
        return f"I heard you say: {latest}"[:600]

    async def speak(self, text: str) -> SpeechResult:
        # An audible 660 Hz tone validates the speaker path without a TTS credential.
        sample_rate = 24_000
        tone_frames = sample_rate // 5
        silence_frames = sample_rate // 10
        samples = [int(5000 * math.sin(2 * math.pi * 660 * i / sample_rate)) for i in range(tone_frames)]
        samples.extend([0] * silence_frames)
        return SpeechResult(pcm=struct.pack(f"<{len(samples)}h", *samples))


class OpenAIProvider:
    def __init__(self, settings: Settings):
        self.client = AsyncOpenAI(
            api_key=settings.openai_api_key,
            timeout=settings.request_timeout_seconds,
        )
        self.settings = settings

    async def transcribe(self, wav_data: bytes) -> str:
        result = await self.client.audio.transcriptions.create(
            model=self.settings.openai_stt_model,
            file=("recording.wav", wav_data, "audio/wav"),
        )
        return result.text.strip()

    async def reply(self, messages: list[dict[str, str]]) -> str:
        instructions = messages[0]["content"] if messages and messages[0]["role"] == "system" else ""
        input_messages = messages[1:] if instructions else messages
        result = await self.client.responses.create(
            model=self.settings.openai_model,
            instructions=instructions,
            input=input_messages,
            max_output_tokens=300,
        )
        return result.output_text.strip()

    async def speak(self, text: str) -> SpeechResult:
        response = await self.client.audio.speech.create(
            model=self.settings.openai_tts_model,
            voice=self.settings.openai_tts_voice,
            input=text[:4000],
            response_format="pcm",
        )
        return SpeechResult(pcm=response.content)


def create_provider(settings: Settings) -> VoiceProvider:
    if settings.ai_mode.lower() == "mock":
        return MockProvider()
    if settings.ai_mode.lower() == "openai":
        if not settings.openai_api_key:
            raise RuntimeError("OPENAI_API_KEY is required when AI_MODE=openai")
        return OpenAIProvider(settings)
    raise RuntimeError("AI_MODE must be 'mock' or 'openai'")

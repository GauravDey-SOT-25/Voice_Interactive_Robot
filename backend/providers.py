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
        # Valid 24 kHz mono PCM silence; mock mode exercises transport without a TTS credential.
        return SpeechResult(pcm=b"\x00\x00" * 2400)


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
        result = await self.client.chat.completions.create(
            model=self.settings.openai_model,
            messages=messages,
            max_tokens=300,
        )
        return (result.choices[0].message.content or "").strip()

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

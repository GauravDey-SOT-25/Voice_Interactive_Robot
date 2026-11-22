from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from backend.config import Settings
from backend.providers import MockProvider, OpenAIProvider, create_provider


@pytest.mark.asyncio
async def test_mock_provider_contract():
    provider = MockProvider()
    assert await provider.transcribe(b"wav") == "Hello robot"
    assert await provider.reply([{"role": "user", "content": "Hi"}]) == "I heard you say: Hi"
    result = await provider.speak("hello")
    assert result.sample_rate == 24_000 and len(result.pcm) == 7200 * 2
    assert result.pcm != b"\x00" * len(result.pcm)


def test_openai_mode_requires_server_side_key():
    with pytest.raises(RuntimeError, match="OPENAI_API_KEY"):
        create_provider(Settings(ai_mode="openai", openai_api_key=""))


@pytest.mark.asyncio
async def test_openai_adapter_uses_configured_models_and_raw_pcm():
    provider = OpenAIProvider(Settings(
        ai_mode="openai",
        openai_api_key="test-only",
        openai_model="configured-chat-model",
        openai_stt_model="configured-stt-model",
        openai_tts_model="configured-tts-model",
        openai_tts_voice="sage",
    ))
    transcribe = AsyncMock(return_value=SimpleNamespace(text="heard words"))
    respond = AsyncMock(return_value=SimpleNamespace(output_text="robot reply"))
    speak = AsyncMock(return_value=SimpleNamespace(content=b"\x01\x00"))
    provider.client = SimpleNamespace(
        audio=SimpleNamespace(
            transcriptions=SimpleNamespace(create=transcribe),
            speech=SimpleNamespace(create=speak),
        ),
        responses=SimpleNamespace(create=respond),
    )

    assert await provider.transcribe(b"wav") == "heard words"
    assert await provider.reply([
        {"role": "system", "content": "robot prompt"},
        {"role": "user", "content": "hello"},
    ]) == "robot reply"
    audio = await provider.speak("hello")
    assert audio.pcm == b"\x01\x00" and audio.sample_rate == 24_000
    transcribe.assert_awaited_once()
    assert respond.await_args.kwargs["model"] == "configured-chat-model"
    assert respond.await_args.kwargs["instructions"] == "robot prompt"
    assert respond.await_args.kwargs["input"] == [{"role": "user", "content": "hello"}]
    assert speak.await_args.kwargs["model"] == "configured-tts-model"
    assert speak.await_args.kwargs["voice"] == "sage"
    assert speak.await_args.kwargs["response_format"] == "pcm"

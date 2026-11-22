import pytest

from backend.config import Settings
from backend.providers import MockProvider, create_provider


@pytest.mark.asyncio
async def test_mock_provider_contract():
    provider = MockProvider()
    assert await provider.transcribe(b"wav") == "Hello robot"
    assert await provider.reply([{"role": "user", "content": "Hi"}]) == "I heard you say: Hi"
    result = await provider.speak("hello")
    assert result.sample_rate == 24_000 and len(result.pcm) % 2 == 0


def test_openai_mode_requires_server_side_key():
    with pytest.raises(RuntimeError, match="OPENAI_API_KEY"):
        create_provider(Settings(ai_mode="openai", openai_api_key=""))

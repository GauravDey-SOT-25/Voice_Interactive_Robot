import io
import wave

import pytest

from backend.audio import pcm16_to_wav, validate_wav


def test_pcm16_to_wav_round_trip_metadata():
    wav_bytes = pcm16_to_wav(b"\x01\x00" * 160, sample_rate=16_000)
    assert validate_wav(wav_bytes) == (16_000, 1, 160)


def test_validate_wav_rejects_non_target_sample_rate():
    data = io.BytesIO()
    with wave.open(data, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(8_000)
        wav.writeframes(b"\x00\x00" * 50)
    with pytest.raises(ValueError, match="16 kHz"):
        validate_wav(data.getvalue())


def test_validate_wav_rejects_truncated_data():
    with pytest.raises(ValueError):
        validate_wav(b"RIFF\x00\x00\x00\x00WAVE")

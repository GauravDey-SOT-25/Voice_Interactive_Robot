import io
import wave

from fastapi import HTTPException, UploadFile


def pcm16_to_wav(pcm: bytes, sample_rate: int = 24_000, channels: int = 1) -> bytes:
    if not pcm or len(pcm) % (2 * channels):
        raise ValueError("PCM must contain complete signed 16-bit frames")
    out = io.BytesIO()
    with wave.open(out, "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(pcm)
    return out.getvalue()


def validate_wav(data: bytes) -> tuple[int, int, int]:
    try:
        with wave.open(io.BytesIO(data), "rb") as wav:
            channels, width, rate, frames = (
                wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()
            )
            if channels != 1 or width != 2 or rate != 16_000 or frames <= 0:
                raise ValueError("Expected non-empty 16 kHz mono signed 16-bit PCM WAV")
            if wav.getcomptype() != "NONE":
                raise ValueError("Compressed WAV is not supported")
            return rate, channels, frames
    except (wave.Error, EOFError) as exc:
        raise ValueError("Invalid WAV file") from exc


async def read_wav_upload(upload: UploadFile, max_bytes: int) -> bytes:
    data = await upload.read(max_bytes + 1)
    if len(data) > max_bytes:
        raise HTTPException(status_code=413, detail="Audio payload exceeds configured limit")
    try:
        validate_wav(data)
    except ValueError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc
    return data

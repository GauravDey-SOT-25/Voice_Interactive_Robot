"""Credential-gated spoken-turn smoke test for every configured live provider.

Set ROBOT_LIVE_TEST_WAV to a 16 kHz, mono, signed 16-bit PCM WAV containing a
short spoken request. Provider keys stay in the server environment; the WAV is
sent to each configured cloud provider by this script.
"""

from __future__ import annotations

import asyncio
import os
import wave
from pathlib import Path

from backend.config import get_settings
from backend.live_providers import build_live_providers


async def check_provider(name, provider, pcm: bytes, timeout: float) -> bool:
    session = await provider.connect(get_settings().system_prompt)
    received_audio = 0
    event_task = asyncio.create_task(collect_audio(session))
    try:
        frame_bytes = 640  # 20 ms at 16 kHz, mono PCM16.
        for offset in range(0, len(pcm), frame_bytes):
            await session.send_audio(pcm[offset : offset + frame_bytes])
            await asyncio.sleep(0.020)
        received_audio = await asyncio.wait_for(event_task, timeout=timeout)
    finally:
        if not event_task.done():
            event_task.cancel()
            await asyncio.gather(event_task, return_exceptions=True)
        await session.close()
    print(f"{name}: {'PASS' if received_audio else 'FAIL'} (received {received_audio} audio bytes)")
    return received_audio > 0


async def collect_audio(session) -> int:
    byte_count = 0
    async for event in session.receive():
        if event.kind == "audio":
            byte_count += len(event.data)
            if byte_count >= 4800:
                return byte_count
    return byte_count


async def main() -> int:
    settings = get_settings()
    configured = [
        item
        for item in build_live_providers(settings).items()
        if item[0] != "mock_live" and item[1].configured()
    ]
    if not configured:
        print("SKIP: no live-provider API credentials are configured")
        return 0
    fixture = os.getenv("ROBOT_LIVE_TEST_WAV", "")
    if not fixture:
        print("SKIP: set ROBOT_LIVE_TEST_WAV to a spoken 16 kHz mono PCM16 WAV")
        return 0
    with wave.open(str(Path(fixture)), "rb") as audio:
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, 16_000):
            raise SystemExit("Fixture must be mono, PCM16, 16 kHz WAV")
        pcm = audio.readframes(audio.getnframes())
    if not pcm:
        raise SystemExit("Fixture contains no audio")

    outcomes = await asyncio.gather(
        *(check_provider(name, provider, pcm, settings.live_session_timeout_seconds * 3)
          for name, provider in configured),
        return_exceptions=True,
    )
    failures = 0
    for (name, _), outcome in zip(configured, outcomes):
        if isinstance(outcome, BaseException):
            failures += 1
            print(f"{name}: FAIL ({type(outcome).__name__})")
        elif not outcome:
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))

# Verification record

Environment on 2026-11-22:

- Backend: Python 3.14.7, FastAPI/OpenAI SDK installed from `pyproject.toml`.
- `python3 -m pytest -q`: **10 passed**. Includes audio WAV validation, mock and OpenAI adapter tests (stubbed, no network request), API route/audio format checks, device auth, and SQLite history.
- `ruff check backend`: **passed**.
- Firmware: Arduino CLI bundled with Arduino IDE 2.3.11, Arduino-ESP32 3.3.12, `ESP32S3 Dev Module`, 16 MB flash, 16M 3MB APP/9.9MB FATFS partition, OPI PSRAM, USB CDC on. **Continuous voice firmware compile succeeded**: 2,424,039 bytes program storage (77% of 3,145,728), 61,612 bytes global RAM (18% of 327,680). This includes ESP-SR AFE, full-duplex AEC, WebRTC VAD, and adaptive noise gate.

The IDE reported no T4-S3 serial device connected, so firmware upload and physical microphone, amplifier, TLS, Wi-Fi, VAD, echo cancellation, barge-in, and end-to-end robot behavior remain unverified. GPIO41/42 are candidate speaker clocks and the exact user's PCB and breakout revisions are not known; confirm the matching pinout/schematic and electrical ratings before wiring. The AEC playback reference uses a software 24-to-16 kHz resampler; enclosure acoustic delay and thresholds need physical tuning. The OpenAI provider was not called because no API key was supplied; the tested mock path is offline.

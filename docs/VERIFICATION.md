# Verification record

Environment on 2026-11-22:

- Backend: Python 3.14.7, FastAPI/OpenAI SDK installed from `pyproject.toml`.
- `python3 -m pytest -q`: **10 passed**. Includes audio WAV validation, mock and OpenAI adapter tests (stubbed, no network request), API route/audio format checks, device auth, and SQLite history.
- `ruff check backend`: **passed**.
- Firmware: Arduino IDE 2.3.10, Arduino-ESP32 3.3.12, `ESP32S3 Dev Module`, 16 MB flash, 16M 3MB APP/9.9MB FATFS partition, OPI PSRAM, USB CDC on. **Compile succeeded**: 1,023,767 bytes program storage (32% of 3,145,728), 47,276 bytes global RAM (14% of 327,680).

The IDE reported no T4-S3 serial device connected, so firmware upload and physical microphone, amplifier, TLS, Wi-Fi, and end-to-end robot behavior remain unverified. The exact user's PCB and breakout revisions are not known; confirm the appropriate pinout/schematic and electrical ratings before wiring. The OpenAI provider was not called because no API key was supplied; the tested mock path is offline.

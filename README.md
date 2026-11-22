# Voice Interactive Robot

Arduino IDE firmware and a Python/FastAPI voice service for a hands-free, continuously listening robot built around the LILYGO T4-S3, INMP441 microphone, and MAX98357A amplifier.

The backend runs offline in mock mode by default. OpenAI credentials are only read by the server. The public repository must never contain a real `.env`, device token, API key, audio recording, or conversation database.

## What works in this first implementation

- FastAPI health, transcription, text chat, speech, complete voice, and robot-oriented raw-PCM endpoints.
- SQLite conversation history, optional bearer-token device authentication, payload limits, WAV validation, and provider timeouts.
- Mock provider for repeatable local development (including a speaker-test tone) and an OpenAI STT/chat/TTS provider.
- Arduino IDE C++ firmware that starts listening at boot, uses ESP-SR full-duplex AEC and WebRTC VAD, captures phrases automatically, uploads WAV audio, streams responses, supports barge-in, and returns to listening without a button or wake word.

Firmware compilation and real-board I2S/audio behavior depend on the installed Arduino-ESP32 3.x core and physical wiring. See [hardware connections](docs/HARDWARE.md) before connecting audio hardware.

## Backend setup

Requires Python 3.11 or newer.

```sh
cd /Users/gaurav/Voice_Interactive_Robot
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -e '.[dev]'
cp .env.example .env
uvicorn backend.app:app --host 0.0.0.0 --port 8000 --reload
```

Run checks with `pytest` and `ruff check backend`.

Mock mode needs no API key. To use OpenAI, set `AI_MODE=openai` and `OPENAI_API_KEY` in the server's untracked `.env`. Never put provider credentials in firmware. Add a long random `DEVICE_AUTH_TOKEN` to the server and matching firmware configuration before exposing the API beyond a trusted development network. Use HTTPS with a trusted certificate in deployment.

The OpenAI adapter uses the Responses API, `gpt-transcribe`, and the standalone Speech API. OpenAI currently schedules its Speech API models, including `gpt-4o-mini-tts`, to shut down on January 6, 2027; plan to move the `VoiceProvider.speak` adapter to the Realtime API or another provider before that date. No OpenAI request was made during local verification.

Useful calls (server defaults to mock):

```sh
curl http://localhost:8000/health
curl -X POST http://localhost:8000/v1/chat -H 'Content-Type: application/json' -d '{"text":"Hello robot","conversation_id":"demo"}'
curl -X POST http://localhost:8000/v1/speech -H 'Content-Type: application/json' -d '{"text":"Hello"}' --output reply.pcm
```

OpenAPI docs are at `http://localhost:8000/docs`.

## Firmware setup

Open `firmware/VoiceInteractiveRobot/VoiceInteractiveRobot.ino` in Arduino IDE. Use Arduino-ESP32 3.x (the I2S driver uses the IDF 5 standard-mode API) and set the following options, matching LILYGO's T4-S3 guide:

| Arduino IDE setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240 MHz (WiFi) |
| Flash Mode / Size | QIO 80 MHz / 16 MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Upload Mode / Speed | UART0 / Hardware CDC / 921600 |
| USB Mode | Hardware CDC and JTAG |

Set Wi-Fi, API URL, bearer token, and trusted root CA in `include/Secrets.h` (this local-only file is ignored by Git; start from `Secrets.example.h`). Compile and upload over USB-C. After boot, speech capture starts automatically. Silence finalizes a phrase after 800 ms by default; the robot plays the response and resumes listening automatically. A continuous user utterance during playback stops the current response and is processed as the next turn.

The firmware calls `POST /v1/voice/audio` with `multipart/form-data` field `file` containing 16 kHz mono signed 16-bit PCM WAV. It expects raw 24 kHz mono signed 16-bit little-endian PCM and metadata headers. Use an HTTPS endpoint with a trusted certificate; a private LAN deployment still needs a certificate trusted by the ESP32.

Silence timeout, utterance bounds, pre-roll, barge-in confirmation, retry backoff, sample rates, and candidate GPIO assignments are centralized in `firmware/VoiceInteractiveRobot/include/RobotConfig.h`. Two I2S peripherals are used so microphone capture and speaker output remain concurrent for echo cancellation and barge-in.

## Layout

- `firmware/VoiceInteractiveRobot/`: Arduino IDE sketch and modular headers/sources.
- `backend/`: API, settings, audio helpers, SQLite store, and provider adapters.
- `docs/`: API contract, board and wiring details, architecture, and troubleshooting.

See [API](docs/API.md), [hardware](docs/HARDWARE.md), [architecture](docs/ARCHITECTURE.md), [troubleshooting](docs/TROUBLESHOOTING.md), and [verification results](docs/VERIFICATION.md).

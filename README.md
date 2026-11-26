# Voice Interactive Robot

Arduino IDE firmware and a Python/FastAPI voice service for a hands-free, continuously listening robot built around the LILYGO T4-S3, INMP441 microphone, and MAX98357A amplifier.

The backend runs offline in mock mode by default. OpenAI credentials are only read by the server. The public repository must never contain a real `.env`, device token, API key, audio recording, or conversation database.

## What works in this first implementation

- FastAPI health, transcription, text chat, speech, complete voice, and robot-oriented raw-PCM endpoints.
- SQLite conversation history, optional bearer-token device authentication, payload limits, WAV validation, and provider timeouts.
- Mock provider for repeatable local development (including a speaker-test tone) and an OpenAI STT/chat/TTS provider.
- Arduino IDE C++ firmware that starts listening at boot, uses ESP-SR full-duplex AEC and WebRTC VAD, streams enhanced microphone PCM continuously over WebSocket, plays live speech audio as it arrives, and supports barge-in without a button or wake word.
- Live speech-to-speech adapters for OpenAI Realtime and Gemini Live, with server-side provider credentials and automatic startup fallback. `mock_live` supports local protocol checks only; it emits a test tone rather than synthesized speech.

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

Mock mode needs no API key. To use live speech, set `OPENAI_API_KEY` and/or `GOOGLE_API_KEY` in the server's untracked `.env`. `LIVE_PROVIDER_ORDER` controls startup fallback order (default: OpenAI Realtime, then Gemini Live). The live route is `/v1/voice/live`; health reports which live providers have credentials configured. A provider is only considered available when configured, and successful real-model operation still depends on valid credentials, account access, and network access. Never put provider credentials in firmware. Add a long random `DEVICE_AUTH_TOKEN` to the server and matching firmware configuration before exposing the API beyond a trusted development network. Use HTTPS with a trusted certificate in deployment.

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

The firmware keeps a WebSocket open to `/v1/voice/live?provider=auto`, sends enhanced 16 kHz mono signed 16-bit little-endian PCM frames as binary messages, and receives 24 kHz mono PCM frames. JSON control messages announce the stream and signal interruption. Use a WSS endpoint with a certificate trusted by the ESP32. The older HTTP voice endpoints remain available for clients that use turn-based audio uploads.

Install the `WebSockets` Arduino library (version 2.7.2 or newer) through Arduino IDE Library Manager. The Python `google-genai` and `websockets` packages are installed with the project dependencies. Run `pytest` for the local FastAPI and stream bridge checks. Live cloud smoke checks require valid provider keys and a spoken-audio fixture; they cannot be counted as passed by the local mock test.

OpenAI server-side VAD silence duration, speech threshold, and prefix padding are set with `LIVE_SILENCE_DURATION_MS`, `LIVE_VAD_THRESHOLD`, and `LIVE_PREFIX_PADDING_MS` in the backend `.env`. On-device VAD/AEC, noise filtering, barge-in confirmation, sample rates, and candidate GPIO assignments are centralized in `firmware/VoiceInteractiveRobot/include/RobotConfig.h`. Two I2S peripherals keep microphone capture and speaker output concurrent for echo cancellation and barge-in.

## Layout

- `firmware/VoiceInteractiveRobot/`: Arduino IDE sketch and modular headers/sources.
- `backend/`: API, settings, audio helpers, SQLite store, and provider adapters.
- `docs/`: API contract, board and wiring details, architecture, and troubleshooting.

See [API](docs/API.md), [hardware](docs/HARDWARE.md), [architecture](docs/ARCHITECTURE.md), [troubleshooting](docs/TROUBLESHOOTING.md), and [verification results](docs/VERIFICATION.md).

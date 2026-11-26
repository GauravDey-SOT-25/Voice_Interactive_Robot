# Arduino IDE firmware

Open `VoiceInteractiveRobot.ino` directly in Arduino IDE with Arduino-ESP32 3.x. The sketch uses the IDF 5 standard I2S driver, ESP-SR AFE, and the Arduino WebSockets library. Install the WebSockets library in Arduino IDE Library Manager (`WebSockets` by Markus Sattler, version 2.7.2 or newer).

Copy `include/Secrets.example.h` to `include/Secrets.h` and set Wi-Fi credentials, the live WebSocket host/port/path, device bearer token, and trusted root certificate. `Secrets.h` is ignored by Git. The firmware connects to `/v1/voice/live?provider=auto` over verified TLS.

At boot, the INMP441 microphone and AFE start immediately. On-device WebRTC VAD and ESP-SR AEC remain active while enhanced 16 kHz mono PCM streams to FastAPI. The backend opens one bidirectional live speech session; generated 24 kHz mono PCM is streamed to the MAX98357A speaker. Sustained VAD speech during playback stops local playback and sends an interrupt event. The microphone stream stays open throughout.

No push-to-talk button or wake word is used. A real live provider API key must be configured in the backend `.env`. For offline firmware/API protocol checks, `AI_MODE=mock` uses a local mock stream that returns a test tone; this is not a conversational model.

Settings for the VAD/AEC audio pipeline, barge-in confirmation, and I2S pins are in `include/RobotConfig.h`; see `../../docs/HARDWARE.md` before wiring hardware. Physical audio quality, echo suppression, and barge-in thresholds need validation on the assembled board and speaker enclosure.

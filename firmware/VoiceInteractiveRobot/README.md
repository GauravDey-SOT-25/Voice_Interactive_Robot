# Arduino IDE firmware

Open `VoiceInteractiveRobot.ino` directly in Arduino IDE with Arduino-ESP32 3.x. The sketch uses the IDF 5 standard I2S driver and the ESP-SR AFE bundled with the installed ESP32 core. Select the ESP32S3 Dev Module with 16 MB flash, OPI PSRAM, USB CDC on boot, and the T4-S3 settings listed in the root README.

Copy `include/Secrets.example.h` to `include/Secrets.h` and set Wi-Fi credentials, an HTTPS `/v1/voice/audio` endpoint, the server device token, and a trusted root certificate. `Secrets.h` is ignored by Git. The firmware verifies TLS.

At boot, the microphone and AFE start before any network request. On-device WebRTC VAD continuously detects phrases with pre-roll, a configurable silence timeout, minimum and maximum duration, and two PSRAM capture buffers. An adaptive noise gate follows the quiet-room noise floor and suppresses low-level audio. A worker task uploads a completed utterance and streams the reply while the listener continues to process microphone/reference audio. ESP-SR full-duplex AEC uses the speaker PCM as the reference. Sustained speech during playback requests barge-in; once the response stream is stopped, that phrase is queued as the next turn. Failures retry with capped exponential backoff while listening remains active.

No push-to-talk button or wake word is used. Settings are in `include/RobotConfig.h`; wiring and the second I2S clock pair are described in `../../docs/HARDWARE.md`. GPIO41/42 are candidate speaker clocks and must be verified against the exact PCB revision and header before wiring. Physical barge-in and echo performance require tuning in the actual enclosure.

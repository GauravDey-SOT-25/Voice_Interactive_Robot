# Arduino IDE firmware

Open `VoiceInteractiveRobot.ino` directly in Arduino IDE with Arduino-ESP32 3.x (IDF 5 standard-mode I2S). Set board to ESP32S3 Dev Module, USB CDC On Boot Enabled, CPU 240 MHz (WiFi), Flash QIO 80 MHz / 16 MB, PSRAM OPI, partition `16M Flash (3MB APP/9.9MB FATFS)`, Upload Mode `UART0 / Hardware CDC` at 921600, USB Mode `Hardware CDC and JTAG`. Install ArduinoJson 6 only if expanding the firmware to consume JSON endpoints; the current streaming binary client does not require it.

Copy `include/Secrets.example.h` to `include/Secrets.h` and configure Wi-Fi credentials, an HTTPS `/v1/voice/audio` endpoint, a server device token, and PEM root certificate. `Secrets.h` is ignored by Git. The firmware verifies TLS and will not connect with an empty/untrusted CA.

Hold the T4-S3 BOOT button to capture mono 16 kHz audio, up to ten seconds. Release to send the WAV in a streamed multipart request. The robot then streams signed 16-bit little-endian mono 24 kHz response PCM into I2S output. The I2S port is reinitialized between RX and TX. Firmware source is modular so capture, WAV encoding, and network orchestration can be changed independently.

The audio buses and board pin allocation are described in `../../docs/HARDWARE.md`. Compile and upload through Arduino IDE using the official T4-S3 board options; no PlatformIO project is included.

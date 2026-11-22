# Troubleshooting

- **401 from the service:** send the same bearer token configured in server `.env` and local ignored firmware `Secrets.h`.
- **422 audio error:** microphone uploads must contain a valid PCM WAV, mono, 16 kHz, 16-bit. Confirm that I2S slot selection matches the INMP441 L/R pin.
- **504 / 502:** check backend logs, server connectivity, and provider credentials. Try `AI_MODE=mock` first.
- **No microphone samples:** check 3.3 V, common ground, BCLK/WS/DOUT wiring, and mic channel select. Do not power the INMP441 from 5 V.
- **Amplifier silent or distorted:** check MAX98357A supply/current, common ground, DIN/BCLK/WS, speaker impedance, and PCM metadata. Do not drive the speaker from an ESP32 GPIO.
- **TLS connection fails:** install a trusted TLS certificate and place the issuing root CA in local `Secrets.h`; TLS verification is intentionally enabled.
- **Arduino I2S symbols unavailable:** install/select Arduino-ESP32 3.x, which bundles ESP-IDF 5's standard I2S driver. This project has not been compile-verified against an installed board package in this environment yet.

# T4-S3 hardware and wiring

## Board review

Target is LILYGO T4-S3 (ESP32-S3R8), not another T4 or T-Display-S3. LILYGO's [board guide](https://wiki.lilygo.cc/products/t4-series/t4-s3/) documents the AMOLED/touch display, SD card, expansion header and Arduino IDE settings. The [official AMOLED Series pin map](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series#-hardware-description) assigns GPIO0 to BOOT, GPIO1-4 to SD, GPIO5 to PMU IRQ, GPIO6/7 to shared touch/PMU I2C, GPIO8/17 to touch, GPIO9-16 to display, GPIO18 to display TE, GPIO19/20 to USB and GPIO43/44 to UART. Do not use those for audio. The official pin map omits unused header signals; the candidate GPIO21, GPIO38, GPIO47 and GPIO48 assignments are cross-checked against the [T4-S3 pinout diagram](https://wiki.lilygo.cc/products/t4-series/t4-s3/index/image/t4-s3-pinout.jpg) and [T4-S3-240719 schematic](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/blob/master/schematic/T4-S3-240719.pdf). Header availability and the exact revision must still be checked on the physical board before assembly.

The firmware assigns GPIO21/38/47 to I2S0 microphone BCLK/WS/data and GPIO41/42/48 to I2S1 speaker BCLK/WS/data. This keeps microphone capture running while speaker output plays, which is required for the AEC reference and barge-in. GPIO41/42 are candidate free header pins based on the available T4-S3 pinout; verify them on the exact PCB revision against the official schematic before wiring. The firmware does not use the BOOT button. Never connect audio modules while powered.

The [INMP441 datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf) specifies 24-bit two's-complement I2S data, MSB first, with 32 SCK cycles per channel word and 64 clocks per WS stereo frame. Firmware requests 32-bit left-channel slots, then shifts the left-justified samples down to signed 16-bit PCM. The INMP441 L/R pin is tied low for the left slot in the table below. Re-check this alignment on the actual microphone breakout before relying on level accuracy.

## Connections

| Signal | T4-S3 | INMP441 | MAX98357A |
| --- | --- | --- | --- |
| I2S0 microphone bit clock | GPIO21 | SCK | — |
| I2S0 microphone word select | GPIO38 | WS | — |
| I2S0 capture data | GPIO47 | SD | — |
| I2S1 speaker bit clock | GPIO41 | — | BCLK |
| I2S1 speaker word select | GPIO42 | — | LRC |
| I2S1 playback data | GPIO48 | — | DIN |
| Supply | regulated 3.3 V | VDD | VIN per breakout specification |
| Ground | GND | GND | GND |

Tie INMP441 L/R to GND for left channel (or to 3.3 V and change slot selection). Confirm the breakout's voltage range before power-up. MAX98357A breakouts vary: many accept 2.7-5.5 V VIN. Use a regulated supply with adequate transient current for the amplifier and speaker; the T4-S3 USB/3.3 V rail is not assumed to power motors or a loud speaker. Keep motor current paths separate from microphone/amplifier analog ground routing and join grounds at a deliberate common point. Use a compatible 4-ohm speaker only if the selected breakout supports it.

## Power and verification limits

Keep the board powered through USB-C or the specified battery input. Power motors from a separate regulated motor supply and use an appropriately rated motor driver. Tie logic grounds together; do not route motor current through the T4-S3 or microphone ground path. Verify supply voltage and current limits from the exact module documentation before powering.

The repository does not identify the user's PCB revision, inspect the physical header, determine the breakout-board revision, or validate the audio modules. GPIO41 and GPIO42 being candidates is not proof of their availability on every board revision. Confirm those details and continuity against the matching LILYGO schematic before soldering. Prototype first with current-limited supplies and no motors attached.

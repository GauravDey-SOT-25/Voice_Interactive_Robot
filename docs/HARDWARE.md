# T4-S3 hardware and wiring

## Board review

Target is LILYGO T4-S3 (ESP32-S3R8), not another T4 or T-Display-S3. LILYGO's [board guide](https://wiki.lilygo.cc/products/t4-series/t4-s3/) documents the AMOLED/touch display, SD card, expansion header and Arduino IDE settings. The [official AMOLED Series pin map](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series#-hardware-description) assigns GPIO0 to BOOT, GPIO1-4 to SD, GPIO5 to PMU IRQ, GPIO6/7 to shared touch/PMU I2C, GPIO8/17 to touch, GPIO9-16 to display, GPIO18 to display TE, GPIO19/20 to USB and GPIO43/44 to UART. Do not use those for audio. The official pin map omits unused header signals; the candidate GPIO21, GPIO38, GPIO47 and GPIO48 assignments are cross-checked against the [T4-S3 pinout diagram](https://wiki.lilygo.cc/products/t4-series/t4-s3/index/image/t4-s3-pinout.jpg) and [T4-S3-240719 schematic](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/blob/master/schematic/T4-S3-240719.pdf). Header availability and the exact revision must still be checked on the physical board before assembly.

The firmware assigns GPIO21=BCLK, GPIO38=WS/LRCLK, GPIO47=mic data input, GPIO48=amplifier data output. BCLK/WS are time-multiplexed: firmware initializes the I2S port for RX during recording, then tears down and reinitializes it for TX during playback. GPIO0 BOOT is used as an active-low push-to-talk button. Check the schematic matching your PCB revision and use the official pinout image. Never connect audio modules while powered.

The [INMP441 datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf) specifies 24-bit two's-complement I2S data, MSB first, with 32 SCK cycles per channel word and 64 clocks per WS stereo frame. Firmware requests 32-bit left-channel slots, then shifts the left-justified samples down to signed 16-bit PCM. The INMP441 L/R pin is tied low for the left slot in the table below. Re-check this alignment on the actual microphone breakout before relying on level accuracy.

## Connections

| Signal | T4-S3 | INMP441 | MAX98357A |
| --- | --- | --- | --- |
| I2S bit clock | GPIO21 | SCK | BCLK |
| I2S word select | GPIO38 | WS | LRC |
| Capture data | GPIO47 | SD | — |
| Playback data | GPIO48 | — | DIN |
| Supply | regulated 3.3 V | VDD | VIN per breakout specification |
| Ground | GND | GND | GND |

Tie INMP441 L/R to GND for left channel (or to 3.3 V and change slot selection). Confirm the breakout's voltage range before power-up. MAX98357A breakouts vary: many accept 2.7-5.5 V VIN. Use a regulated supply with adequate transient current for the amplifier and speaker; the T4-S3 USB/3.3 V rail is not assumed to power motors or a loud speaker. Keep motor current paths separate from microphone/amplifier analog ground routing and join grounds at a deliberate common point. Use a compatible 4-ohm speaker only if the selected breakout supports it.

## Power and verification limits

Keep the board powered through USB-C or the specified battery input. Power motors from a separate regulated motor supply and use an appropriately rated motor driver. Tie logic grounds together; do not route motor current through the T4-S3 or microphone ground path. Verify supply voltage and current limits from the exact module documentation before powering.

The repository does not identify the user's PCB revision, inspect the physical header, determine the breakout-board revision, or validate the audio modules. Confirm those details and continuity against the matching LILYGO schematic before soldering. Prototype first with current-limited supplies and no motors attached.

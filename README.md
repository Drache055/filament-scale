# Filament Scale

[Website](https://drache055.github.io/filament-scale/) · [Build guide](https://drache055.github.io/filament-scale/bauen.html)

![The storage box with a spool inside on the scale](docs/img/hero.jpg)

A filament scale built on the ESP32-S3. It reads the spool and its storage box via NFC, weighs the spool inside the box and writes the remaining weight and the box as location to [Spoolman](https://github.com/Donkie/Spoolman).

## How it works

1. Hold the spool to the scan zone. The scale looks it up in Spoolman.
2. Put the storage box (with its own NFC tag) on the scale. Weight W1 is stored.
3. Put the spool into the box. Weight W2 is stored.
4. Confirm. Remaining weight and location go to Spoolman.

The box weight never has to be known. Example from the device screens:

| | |
|---|---:|
| W2, box with spool | 1254 g |
| − W1, box alone | 412 g |
| − empty spool (from Spoolman) | 250 g |
| **= remaining weight** | **592 g** |

## Features

- NFC tags NTAG215/216 in OpenSpool format; spool, box and user tags can be written on the device
- Uses the native NFC tags of Spoolman 0.27; unknown spools are linked by picking them from the Spoolman list
- Offline queue: up to 100 updates are kept in flash and sent when Spoolman is reachable again
- Users sign in with their own tag and are recorded as spool owner; settings are admin only
- Web interface for calibration, users, filament catalog, Spoolman, Wi-Fi, log and system settings
- Wi-Fi setup through a hotspot with captive portal, firmware updates over Wi-Fi (OTA)
- German and English, language per user

## Hardware

| Part | Details |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-2 (16 MB flash, 8 MB PSRAM, 240×320 touch display) |
| Load cell | 5 kg bar load cell |
| ADC | HX711, powered with 3.3 V |
| NFC | PN532 in SPI mode |
| Tags | NTAG215 or NTAG216 |
| Enclosure | 3D printed, steel ballast in the base |

Pinout: [`firmware/include/pins.h`](firmware/include/pins.h). Wiring and mechanics: [build guide](https://drache055.github.io/filament-scale/bauen.html).

## Getting started

**Without a toolchain:** use the [web flasher](https://drache055.github.io/filament-scale/bauen.html#webflash) in Chrome or Edge on a computer.

**With PlatformIO:**

```bash
git clone https://github.com/Drache055/filament-scale.git
cd filament-scale/firmware
cp include/secrets.h.example include/secrets.h   # Spoolman URL, Wi-Fi optional
pio run -e waage -t upload                       # first flash over USB
pio run -e waage -t uploadfs                     # theme data
```

After that, updates go over Wi-Fi. Without Wi-Fi credentials the scale opens the hotspot `Filamentwaage-Setup`.

Unit tests run on the host without hardware: `cd firmware && pio test -e native`

Default login: admin password `Filament`, device PIN `1234`. Change both after the first start.

## Repository

```
firmware/   PlatformIO project (ESP32-S3, Arduino, LVGL 9)
docs/       Website (GitHub Pages) and web flasher
```

Development notes: [`firmware/CLAUDE.md`](firmware/CLAUDE.md).

## Status

The firmware builds and the logic is covered by unit tests. The full weighing flow with a real scale, box tag and Spoolman has not been fully tested on the device yet. Feedback and issues are welcome.

## License

[GPL-3.0](LICENSE). You may use, modify and redistribute the firmware, privately and commercially. If you distribute it, modified or not, or ship it in a product, you must publish the complete source code under GPL-3.0. A different commercial license is available on request.

The bundled libraries (LVGL, LovyanGFX, HX711, Adafruit PN532, ArduinoJson) keep their own licenses. 3D files and documentation have no separate license yet.

# Filament Scale

**🇬🇧 English** · [🇩🇪 Deutsch](#deutsch) · [Website](https://drache055.github.io/filament-scale/)

![Filament Scale](docs/img/hero.jpg)

A smart filament scale built on the ESP32-S3: identify a spool via NFC, identify a storage box via NFC, determine remaining filament by differential weighing and write it to [Spoolman](https://github.com/Donkie/Spoolman) automatically.

## Features

- **NFC identification** of spools (OpenSpool compatible, NTAG215/216) and storage boxes
- **Differential weighing:** weigh the box, put the spool in, weigh again. The box weight (and whatever else is inside) does not matter
- **Spoolman integration:** updates `remaining_weight`, `location` (= box) and owner via API, with an offline queue
- **Touch display** (240×320) for tare, calibration and writing tags on the device
- **Web UI** with user management, roles (admin / user / generic), filament catalog, log, system settings
- **Tasmota-style Wi-Fi setup** (hotspot + captive portal), OTA updates
- German and English, language per user

## Hardware

Waveshare ESP32-S3-Touch-LCD-2, 5 kg bar load cell, HX711, PN532 (SPI mode), NTAG215/216 tags, 3D-printed enclosure. Pinout: [`firmware/include/pins.h`](firmware/include/pins.h).

## Quick start

```bash
git clone https://github.com/Drache055/filament-scale.git
cd filament-scale/firmware
cp include/secrets.h.example include/secrets.h
pio run -e waage -t upload
pio run -e waage -t uploadfs
```

Unit tests run on the host: `pio test -e native`. Default credentials: admin password `Filament`, device PIN `1234`. **Change them right away.**

## Status

The firmware builds and the core logic is unit-tested. The full weighing flow with a real scale, box tag and Spoolman has not been fully verified on hardware yet.

## License

Firmware and software are licensed under the [GNU General Public License v3.0](LICENSE). You may use, modify and redistribute them, privately and commercially. Anyone who distributes them (modified or not) or ships them in a product must release the complete source code under GPL-3.0. A different commercial license is available on request from the author.

Bundled libraries (LVGL, LovyanGFX, HX711, Adafruit PN532, ArduinoJson) keep their own licenses. 3D files and documentation do not have a separate license yet.

---

<a id="deutsch"></a>

# Filamentwaage (Deutsch)

Die Firmware spricht auf dem Gerät Deutsch und Englisch. Hier die deutsche Kurzfassung.

## Funktionen

- **NFC-Erkennung** von Spulen (OpenSpool-kompatibel, NTAG215/216) und Lagerboxen
- **Differenzwägung:** Box wiegen, Spule hineinstellen, nochmal wiegen. Das Gewicht der Box (und was sonst darin liegt) spielt keine Rolle
- **Spoolman-Anbindung:** `remaining_weight`, `location` (= Box) und Besitzer werden per API aktualisiert, mit Offline-Warteschlange
- **Touch-Display** (240×320) mit Tara, Kalibrierung, Tag-Schreiben direkt am Gerät
- **Weboberfläche** mit Benutzerverwaltung, Rollen (Admin / Benutzer / Generic), Filament-Katalog, Log, Systemeinstellungen
- **WLAN-Einrichtung wie bei Tasmota** (Hotspot + Captive Portal), Updates per OTA
- Deutsch und Englisch, Sprache pro Benutzer

## Hardware

| Teil | Details |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-2 (ESP32-S3R8, 16 MB Flash, 8 MB PSRAM, ST7789 240×320, CST816 Touch) |
| Wägezelle | Biegebalken 5 kg |
| ADC | HX711 (3,3 V) |
| NFC | PN532 im SPI-Modus |
| Tags | NTAG215 / NTAG216 |
| Gehäuse | 3D-gedruckt, Ballast aus Stahl im Sockel |

Pinbelegung: [`firmware/include/pins.h`](firmware/include/pins.h). Ausführliche Entwicklungsnotizen: [`firmware/CLAUDE.md`](firmware/CLAUDE.md).

## Schnellstart

Voraussetzung: [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/Drache055/filament-scale.git
cd filament-scale/firmware
cp include/secrets.h.example include/secrets.h   # ausfüllen (WLAN optional, Spoolman-URL)
pio run -e waage -t upload                       # erstes Flashen per USB
pio run -e waage -t uploadfs                     # Theme-Daten
```

Danach geht alles per WLAN (OTA). Ohne WLAN-Daten startet das Gerät den Hotspot `Filamentwaage-Setup`.

Tests auf dem Rechner (ohne Hardware):

```bash
cd firmware && pio test -e native
```

Standard-Zugang: Admin-Passwort `Filament`, Geräte-PIN `1234`. **Bitte direkt ändern.**

## Ablauf

1. Spule ans Gerät halten, Tag wird gelesen, Spoolman liefert die Daten
2. Box (mit Box-Tag) auf die Waage stellen, Gewicht W1 wird erfasst
3. Spule in die Box stellen, Gewicht W2 wird erfasst
4. Restgewicht = (W2 − W1) − Leerspulengewicht, bestätigen, an Spoolman senden

## Repository

```
firmware/   PlatformIO-Projekt (ESP32-S3, LVGL 9, Arduino)
docs/       Webseite (GitHub Pages)
```

## Status

Firmware baut, Logik ist per Unit-Tests abgedeckt. Der komplette Wiegeablauf mit echter Waage, Box-Tag und Spoolman ist noch nicht abschließend am Gerät geprüft. Siehe Abschnitt „Offene Punkte“ in `firmware/CLAUDE.md`.

## Lizenz

Firmware und Software stehen unter der [GNU General Public License v3.0](LICENSE). Du darfst sie privat und kommerziell nutzen, ändern und weitergeben. Wer sie (auch verändert) weitergibt oder in einem Produkt verkauft, muss den vollständigen Quellcode unter der GPL-3.0 offenlegen. Eine abweichende kommerzielle Lizenz ist auf Anfrage beim Autor möglich.

Verwendete Bibliotheken (LVGL, LovyanGFX, HX711, Adafruit PN532, ArduinoJson) behalten ihre eigenen Lizenzen. Für 3D-Dateien und Dokumentation gilt noch keine eigene Lizenz.


# CLAUDE.md – Smarte Filamentwaage mit NFC-Box-Zuordnung und Spoolman

## 1. Ziel

Eine ESP32-S3-basierte Waage, die:
1. eine Filamentspule per NFC-Tag (OpenSpool-Format) identifiziert,
2. eine Lagerbox (Müslibox) per NFC-Tag identifiziert,
3. das Brutto-Gewicht der Spule per **Differenzwägung** ermittelt (Box vorher/nachher),
4. das Filament-Restgewicht berechnet und
5. in **Spoolman** `remaining_weight` und `location` (= Box) der Spule aktualisiert.

Sprache für UI-Texte und Kommentare: **Deutsch**. Bezeichner im Code: Englisch.

---

## 2. Hardware

| Komponente | Details |
|---|---|
| MCU/Display | Waveshare ESP32-S3-Touch-LCD-2 (ESP32-S3R8, 8 MB Octal-PSRAM, 16 MB Flash), ST7789T3 (SPI), 240×320 Hochformat, Touch CST816 (I²C, nur Single-Touch) |
| Wägezelle | Biegebalken **5 kg**, 4 Leiter (E+, E−, A+, A−) |
| ADC | HX711-Modul (grün), versorgt mit **3,3 V** (DOUT darf keine 5 V führen), RATE = 10 Hz |
| NFC | PN532, **SPI-Modus** (DIP: SW1=OFF, SW2=ON), 3,3 V, Software-SPI an eigenen Pins (siehe Pinbelegung). Der I²C-Modus lief am Waagen-Board nicht: SCL blieb dort an allen getesteten Pins (IO2/4/6/13/15 und IO47/48) dauerhaft auf low, am NodeMCU-ESP32 lief dasselbe Modul fehlerfrei. Ursache nicht geklärt. |
| Tags | **NTAG216** (vorhanden); NTAG215 muss ebenfalls unterstützt werden |
| Speicher | **keine SD-Karte**, nur Flash (NVS + LittleFS) |

### Mechanik (3D-gedruckt, für die Software relevant)
- Biegebalken: Sockel an einem Ende, Wiegeplatte am anderen. Distanzstücke sorgen dafür, dass nur die Enden Kontakt haben. Pfeil auf der Zelle = Lastrichtung.
- Ballast (Stahl) nur im Sockel und am Rand. Die Wiegeplatte enthält keinen Ballast, ihr Eigengewicht wird tariert.
- **Ein** PN532 sitzt mittig unter der Wiegeplatte (Kunststoff, möglichst ≤ 10 mm). Um die Antenne herum ist mindestens 3 cm frei von Metall. Realistische Leseweite: 2–4 cm.
- Box-Tags kleben **mittig auf dem Boxboden** und werden gelesen, wenn die Box auf der Platte steht.
- Spulen-Tag wird gelesen, indem die Spule kurz an die markierte Scan-Zone der Platte gehalten wird.
- Folge: Während eines Ablaufs können mehrere Tags in Reichweite sein. Die Software ordnet Tags anhand ihres Typs (Spule/Box) und des aktuellen Zustands zu und ignoriert unerwartete Tags.

### Pinbelegung
- Display/Touch: aus dem Waveshare-Wiki bzw. -Beispiel für ESP32-S3-Touch-LCD-2 übernehmen.
- HX711 (kein I²C, eigene 2-Draht-Schnittstelle): SCK = GPIO17 (Header P1 Pin 5), DOUT = GPIO18 (Header P1 Pin 6). Versorgung 3,3 V = Header P2 Pin 1. Quelle: Schaltplan des Boards.
- PN532 (**SPI**, Software-SPI): SCK = GPIO15, MISO = GPIO13, MOSI = GPIO11, SS = GPIO12 (Header P2, Bild-Nr. 7 bis 10 im Waveshare-Pinout), 3V3 = P2 Pin 1, GND = P2 Pin 2. Der Touch bleibt allein auf seinem I²C-Bus (GPIO47/48). TODO: Ob GPIO11 bis 15 sonst belegt sind (vermutlich SD-Slot, ungenutzt), ist nicht am Schaltplan geprüft.
- Header-Belegung des Boards (Quelle: Schaltplan): P1 = IO2, IO4, IO6, IO16, IO17, IO18, IO21, IO8, IO7, IO10, IO20, IO19, GND, 5V (Pin 1–14). P2 = 3V3, GND, IO43, IO44, IO47, IO48, IO15, IO13, IO11, IO12, IO14, IO9, GND, VBAT (Pin 1–14). IO19/IO20 sind USB und nicht verwendbar.
- Alle Pins zentral in `include/pins.h` definieren, nirgends sonst hart codieren.

---

## 3. Software-Stack

- PlatformIO, Plattform **pioarduino** (arduino-esp32 **3.x**)
- `platformio.ini` (Basis, nicht ohne Grund ändern):

```ini
[env:waage]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.filesystem = littlefs
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DLV_CONF_INCLUDE_SIMPLE
    -Iinclude
monitor_speed = 115200
lib_deps =
    lvgl/lvgl@9.5.0
    lovyan03/LovyanGFX
    bogde/HX711
    adafruit/Adafruit PN532
    bblanchon/ArduinoJson@^7
```

- `stable` im Plattform-Link später durch einen festen Release-Tag ersetzen. LovyanGFX ebenfalls auf eine feste Version pinnen, sobald es läuft.
- **LVGL 9.5.0** (exakt gepinnt). `include/lv_conf.h` aus dem 9.5.0-Template erstellen, keine 8.x-Konfiguration verwenden. Nicht benötigte Features (OpenGL, 3D, Blur) bleiben deaktiviert.
- **LovyanGFX** als Display-/Touch-Treiber (nicht TFT_eSPI). Die LVGL-9-Anbindung (Flush-Callback, Touch-Read-Callback) wird selbst geschrieben. Aus den Waveshare-Beispielen werden nur Pins und Treiber-Init übernommen.
- Die UI wird **handgeschrieben in LVGL-C++** umgesetzt. EEZ Studio dient nur zum Ausprobieren von Layouts, generierter Code wird nicht übernommen.
- **NFC/NDEF:** Adafruit_PN532 für Low-Level-Zugriff (`ntag2xx_ReadPage` / `ntag2xx_WritePage`). Das NDEF-TLV-Parsing und -Schreiben für einen einzelnen MIME-Record wird **selbst implementiert** (`nfc/ndef.cpp`). Die NDEF-Library von Don Coleman wird nicht verwendet, weil sie für die Seeed-PN532-Library gebaut ist.
- HTTP: `HTTPClient` + `WiFiClient`; JSON: ArduinoJson 7.
- Persistenz: `Preferences` (NVS) für Konfiguration und Kalibrierung; LittleFS für die Offline-Warteschlange.

---

## 4. NFC-Tags

### 4.1 Allgemein
- Typ: NTAG215 oder NTAG216 (NFC Forum Type 2, ISO14443A). Ein NDEF-Record, MIME-Type `application/json`.
- **Kapazität nicht hart codieren**, sondern aus dem Capability Container lesen (Page 3, Byte 2, Wert × 8 = NDEF-Bereich in Byte):

| Tag | CC Byte 2 | NDEF-Bereich | User-Pages |
|---|---|---|---|
| NTAG215 | `0x3E` | 496 Byte | 4 – 129 |
| NTAG216 | `0x6D` | 872 Byte | 4 – 225 |

- NDEF-TLV ab Page 4: `0x03`, Länge (1 Byte, bzw. `0xFF` + 2 Byte bei ≥ 255), Record, danach `0xFE` (Terminator).
- Record: Short Record (SR) bei Payload < 256 Byte, TNF = `0x02` (MIME), Type = `application/json`.
- Vor dem Schreiben prüfen: Record + TLV + Terminator müssen in die Kapazität passen. Ist ein Tag gesperrt (Lock-Bytes), gibt es eine Fehlermeldung, und es wird nicht geschrieben.
- UID-Format im Code und in Spoolman: Hex, Großbuchstaben, ohne Trennzeichen (7 Byte → 14 Zeichen).

### 4.2 Spulen-Tag (OpenSpool-kompatibel)
```json
{
  "protocol": "openspool",
  "version": "1.0",
  "type": "PLA",
  "color_hex": "FF5733",
  "brand": "Generic",
  "min_temp": "190",
  "max_temp": "220",
  "spoolman_id": 42
}
```
- Die OpenSpool-Felder bleiben unverändert. `spoolman_id` ist ein **zusätzliches** Feld.
- Zuordnung zu Spoolman in dieser Reihenfolge:
  1. `spoolman_id` vorhanden → direkt `GET /api/v1/spool/{id}`.
  2. Sonst über die Tag-**UID** suchen: `GET /api/v1/spool?tag=<UID>` (Spoolman 0.27 hat native NFC-Tags, siehe 7.4).
  3. Sonst → Screen „Unbekannte Spule“: Spule aus der Spoolman-Liste wählen, danach den Tag in Spoolman verknüpfen (`POST /spool/{id}/tag`) und `spoolman_id` auf den Tag schreiben.

### 4.3 Box-Tag (eigenes Format)
```json
{
  "type": "box",
  "id": "BOX-03",
  "name": "Box 03"
}
```
- `name` wird 1:1 als Spoolman-`location` verwendet.
- Leerer bzw. unbekannter Tag beim Box-Schritt → Screen „Neue Box anlegen“: Nummer per Touch wählen und Tag beschreiben.

### 4.4 Tag-Typ-Erkennung
- JSON enthält `"protocol": "openspool"` → Spule
- JSON enthält `"type": "box"` → Box
- Kein NDEF / kein gültiges JSON → „leer/unbekannt“

---

## 5. Ablauf (Zustandsautomat)

Der Ablauf ist ein expliziter Zustandsautomat (`app/state_machine.cpp`). Jeder Zustand hat einen eigenen Screen. In jedem Zustand gibt es die Schaltfläche **Abbrechen**, die zu `IDLE` zurückführt.

```
IDLE
 └─ Spulen-Tag erkannt ───────────────► SPOOL_IDENTIFIED
SPOOL_IDENTIFIED (Spoolman-Daten laden, anzeigen)
 └─ Waage leer & stabil ──────────────► WAIT_BOX
WAIT_BOX ("Box auf die Waage stellen")
 └─ Gewicht > Schwelle & stabil
    UND Box-Tag gelesen ──────────────► BOX_WEIGHED (W1 speichern)
BOX_WEIGHED → WAIT_SPOOL_INSERT ("Spule in die Box stellen")
 └─ Gewicht > W1 + Schwelle & stabil ─► SPOOL_WEIGHED (W2 speichern)
SPOOL_WEIGHED → CONFIRM (Ergebnis anzeigen: Box, Spule, Restgewicht)
 ├─ [Speichern] ──────────────────────► UPLOAD
 └─ [Abbrechen] ──────────────────────► IDLE
UPLOAD (PATCH an Spoolman bzw. in Warteschlange)
 └─ fertig ───────────────────────────► DONE
DONE ("Box entnehmen")
 └─ Waage leer & stabil ──────────────► IDLE
```

Regeln:
- Ab `WAIT_BOX` werden **Spulen**-Tags ignoriert, auch der bereits gescannte.
- Die Box-ID wird in `WAIT_BOX` festgelegt und danach nicht mehr geändert.
- Timeout pro Wartezustand: konfigurierbar (Standard 120 s), danach zurück nach `IDLE` mit Hinweis.
- In `IDLE` zeigt die Waage das Live-Gewicht (normale Küchenwaagen-Funktion).

---

## 6. Gewichtslogik

### 6.1 Differenzwägung
```
spool_gross      = W2 - W1
empty_spool      = spool.spool_weight
                   ?? spool.filament.spool_weight
                   ?? spool.filament.vendor.empty_spool_weight
                   ?? manuelle Eingabe (Screen)
remaining_weight = spool_gross - empty_spool
```
- Das Boxgewicht muss **nicht** bekannt sein. Der Inhalt der Box (andere Spulen, Trockenmittel) spielt keine Rolle.
- Plausibilitätsprüfung:
  - `remaining_weight < -20 g` → Fehler „Leerspulengewicht prüfen“
  - `-20 g ≤ remaining_weight < 0` → auf 0 setzen, Hinweis anzeigen
  - `remaining_weight > initial_weight + 50 g` (falls bekannt) → Warnung, Speichern nur nach Bestätigung
- Überlast: Gesamtgewicht > 4500 g → Warnung „Überlast“ (Zelle 5 kg).

### 6.2 Stabilitätserkennung
- HX711 mit 10 Hz. Gleitendes Fenster über N = 10 Samples (≈ 1 s).
- Stabil, wenn `max − min ≤ 1,0 g` im Fenster (Schwelle konfigurierbar).
- „Leer“: stabil und `|Gewicht| ≤ 5 g`.
- „Objekt hinzugefügt“: stabil und `Gewicht ≥ Referenz + 50 g`.
- Ausgegeben wird der Mittelwert des stabilen Fensters, gerundet auf 0,1 g intern bzw. 1 g in der Anzeige.

### 6.3 Kalibrierung und Tara
- Tara-Funktion im Menü. Außerdem automatisches Nullen beim Booten, wenn die Waage leer und stabil ist.
- Kalibrierung: Tara, bekanntes Gewicht auflegen, Wert eingeben. Daraus Faktor berechnen und in NVS speichern.
- Drift: Nullpunkt-Nachführung nur in `IDLE`, und nur wenn die Waage leer und stabil ist und die Abweichung kleiner als 2 g ist.

---

## 7. Spoolman-Integration

Basis-URL konfigurierbar, z. B. `http://spoolman.local:7912`. API-Präfix: `/api/v1`.
**Vor der Implementierung die API-Doku der laufenden Instanz unter `<basis>/api/v1/docs` prüfen**, weil sich Feldnamen zwischen Spoolman-Versionen ändern können.

### 7.1 Vorbereitung in Spoolman (manuell durch den Nutzer)
- Das Extra-Feld `nfc_uid` ist **nicht mehr nötig**, weil Spoolman 0.27 NFC-Tags nativ kennt (siehe 7.4). Das Text-Feld `owner` legt der Client bei Bedarf selbst an.

### 7.2 Verwendete Endpunkte
| Zweck | Request |
|---|---|
| Spule laden | `GET /api/v1/spool/{id}` |
| Alle Spulen (für UID-Suche und Auswahlliste) | `GET /api/v1/spool` |
| Gewicht und Box speichern | `PATCH /api/v1/spool/{id}` mit `{"remaining_weight": <float>, "location": "<Box-Name>"}` |
| Tag verknüpfen | `POST /api/v1/spool/{id}/tag` mit `{"uid": "04A1B2C3D4E5F6", "format": "ntag"}` |

- **Extra-Feld-Werte sind JSON-kodierte Strings**, das heißt, der String selbst steht in Anführungszeichen (siehe oben). Beim Lesen entsprechend dekodieren.
- Die UID-Suche läuft clientseitig über die Spulenliste. Ergebnis (UID → ID) in NVS cachen.
- Die Spulenliste ist bei vielen Spulen groß: ArduinoJson mit Filter (`DeserializationOption::Filter`) verwenden und nur benötigte Felder parsen. Puffer im PSRAM anlegen.
- Timeouts: 5 s pro Request. Ein Erfolg muss per HTTP-Status 2xx bestätigt sein, bevor `DONE` angezeigt wird.

### 7.3 Offline-Verhalten
- Ohne WLAN oder bei einem Spoolman-Fehler wird das Update als JSON-Zeile (`spool_id`, `remaining_weight`, `location`, `timestamp`) in `/queue.jsonl` (LittleFS) gespeichert.
- Der Netzwerk-Task arbeitet die Warteschlange ab, sobald die Verbindung wieder steht. Ältere Einträge zur selben `spool_id` werden dabei durch den neuesten ersetzt.
- In der Statusleiste anzeigen: WLAN, Spoolman erreichbar, Anzahl ausstehender Updates.
- Zeitstempel per NTP (`configTime`). Ist noch keine Zeit gesetzt, wird `0` gespeichert.

---

## 8. Architektur

### 8.1 Tasks (FreeRTOS)
| Task | Aufgabe | Kern |
|---|---|---|
| `ui` | LVGL (`lv_timer_handler`), Screens, State Machine | 1 |
| `scale` | HX711 lesen, Filter, Stabilität | 1 |
| `nfc` | PN532 pollen (ca. 4 Hz), Tags parsen | 1 |
| `net` | WLAN, Spoolman-Requests, Offline-Warteschlange | 0 |

- **LVGL ist nicht threadsafe.** LVGL-Funktionen werden ausschließlich im `ui`-Task aufgerufen.
- Datenfluss in die UI: `scale` und `nfc` senden Events über eine `QueueHandle_t` an den `ui`-Task oder schreiben in mutex-geschützte Strukturen, die die UI per `lv_timer` (200 ms) liest.
- Netzwerk-Aufträge: `ui` → Queue → `net`; die Ergebnisse gehen per Queue zurück.
- Der I²C-Bus gehört nur dem Touch (`bus/i2c_bus`, Mutex). Das PN532 hängt per SPI an eigenen Pins und braucht keinen Mutex, es kann den Touch nicht blockieren.

### 8.2 Dateistruktur
```
include/
  pins.h            Pinbelegung
  config.h          Defaults (Schwellen, Timeouts)
  lv_conf.h         LVGL 9.5.0
  secrets.h         WLAN, Spoolman-URL (nicht ins Repo einchecken, .gitignore)
src/
  main.cpp          Init, Task-Start
  app/state_machine.{h,cpp}
  scale/scale.{h,cpp}         HX711, Filter, Stabilität, Kalibrierung
  nfc/nfc.{h,cpp}             PN532-Polling (SPI), UID
  nfc/ndef.{h,cpp}            NDEF-TLV lesen/schreiben (MIME-Record, CC-Auswertung)
  nfc/tags.{h,cpp}            Spool-/Box-Tag parsen und erzeugen
  net/wifi.{h,cpp}
  net/spoolman.{h,cpp}        API-Client
  net/queue.{h,cpp}           Offline-Warteschlange
  storage/settings.{h,cpp}    NVS-Konfiguration
  display/display.{h,cpp}     LovyanGFX-Setup, LVGL-Flush-/Touch-Callbacks
  ui/                         Screens (LVGL-Code, handgeschrieben)
```

### 8.3 Screens (240×320, Hochformat)
1. **Home/IDLE:** Live-Gewicht, Statusleiste, Hinweis „Spule scannen“, Buttons [Tara] [Menü]
2. **Spule erkannt:** Hersteller, Material, Farbe (Farbfeld aus `color_hex`), letztes Restgewicht
3. **Box auflegen:** Live-Gewicht, Box-Name, sobald erkannt
4. **Spule einstellen:** Live-Gewicht relativ zu W1
5. **Bestätigen:** Box, Spule, Restgewicht in g und optional in m; [Speichern] [Abbrechen]
6. **Ergebnis:** gespeichert bzw. in Warteschlange
7. **Menü:** Tara, Kalibrierung, Leerspulengewicht manuell, Tag schreiben (Spule/Box), WLAN/Spoolman-URL, Info

Bedienung: Buttons mindestens 48 px hoch (Single-Touch, kleines Display). Pro Screen nur eine Hauptaktion.

Optionale Längenberechnung:
`laenge_m = remaining_weight / (dichte_g_cm3 * pi * (d_mm/20)^2 * 100)`, mit Dichte und Durchmesser aus `spool.filament`.

---

## 9. Konfiguration (NVS, über das Menü änderbar)
- WLAN SSID/Passwort (für den Anfang aus `secrets.h`)
- Spoolman-Basis-URL
- Kalibrierfaktor, Nulloffset
- Schwellen: Stabilität (g), „leer“ (g), „hinzugefügt“ (g), Timeout (s)

---

## 10. Coding-Regeln
- Module mit klaren Schnittstellen, keine globalen Variablen außerhalb des eigenen Moduls.
- Keine blockierenden `delay()`-Aufrufe im `ui`-Task.
- Größere Puffer (JSON, Listen) im PSRAM anlegen (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`). Den LVGL-Draw-Buffer zuerst im internen DMA-fähigen RAM anlegen (Teilpuffer, z. B. 240 × 40 Pixel) und erst bei Bedarf ins PSRAM verschieben.
- Logging über `Serial` mit Modul-Präfix, z. B. `[SCALE]`, `[NFC]`, `[NET]`.
- Jedes Modul einzeln testbar halten (siehe 11).
- Keine Annahmen über unbekannte Pins oder API-Felder treffen: als `TODO` markieren und nachfragen.

## 11. Inbetriebnahme-Reihenfolge
1. Board-Test: Flash 16 MB / PSRAM ~8 MB über Serial ausgeben
2. Display + Touch (LovyanGFX + LVGL Hello World)
3. HX711: Rohwerte → Kalibrierung → Stabilitätserkennung
4. PN532: UID lesen → CC auslesen → NDEF lesen/schreiben auf NTAG216
5. Spoolman-Client gegen die echte Instanz (GET, dann PATCH auf eine Testspule)
6. Zustandsautomat mit Screens
7. Offline-Warteschlange

## 12. Offene Punkte
- HX711: Verdrahtung prüfen (Rohwerte, Richtung der Zelle), danach kalibrieren
- Spoolman-Version prüfen und die Feldnamen aus Abschnitt 7 gegen `/api/v1/docs` abgleichen
- Leseweite des PN532 durch Wiegeplatte und Boxboden praktisch testen
- Spoolman-Basis-URL und WLAN-Daten in `secrets.h` eintragen

## 13. Updates und UI-Styles
- Erstes Flashen per USB: `pio run -e waage -t upload`. Danach nur noch per WLAN (ArduinoOTA, Hostname/Passwort aus `secrets.h`).
- Firmware: `OTA_PASSWORD=<pw> pio run -e waage-ota -t upload`
- **Geräte-Passwort** (`storage/devicepw.{h,cpp}`, `devicePassword()`): Schutz für Einrichtungs-Hotspot und OTA. Steht in `secrets.h` ein eigenes `OTA_PASSWORD` (mindestens 8 Zeichen, nicht der Platzhalter `bitte-aendern`), gilt dieses. Sonst erzeugt die Waage beim ersten Start ein zufälliges Passwort (10 Zeichen, NVS-Namespace `devpw`, Schlüssel `pw`) und zeigt es auf Home im Hotspot-Betrieb sowie unter Menü → Einstellungen → WiFi an. Der Hotspot ist dadurch immer geschützt. Für `waage-ota` das angezeigte Passwort als `OTA_PASSWORD` übergeben. Grund: Web-Flasher-Images werden ohne eigene `secrets.h` gebaut und dürfen kein öffentlich bekanntes Passwort enthalten. Nicht am Gerät geprüft.
- **Web-Flasher:** `.github/workflows/web-flasher.yml` baut bei jedem Release `firmware.factory.bin` (Bootloader 0x0, Partitionen 0x8000, boot_app0 0xe000, Firmware 0x10000, von PlatformIO zusammengesetzt) mit der Vorlage `secrets.h.example` und legt sie samt `manifest.json` nach `docs/firmware/`. Die Seite `docs/bauen.html` bindet ESP Web Tools ein.
- UI-Style (Farben, Radius, Abstände, Schriftgrößen, Buttonhöhe): `data/theme.json` ändern, dann `OTA_PASSWORD=<pw> pio run -e waage-ota -t uploadfs`. Das ersetzt nur das LittleFS-Image, die Firmware bleibt unverändert. Fehlt die Datei oder ist sie ungültig, gelten die Defaults in `ui/theme.cpp`.
- Verfügbare Schriftgrößen: 14, 20, 28, 48 (Dateien `src/ui/fonts/lv_font_app_<größe>.c`, Montserrat Medium). 14/20/28 enthalten Latin-1 (Umlaute, ß) und die LVGL-Symbole, 48 nur ASCII (Gewicht). Die eingebauten Montserrat-Größen 20/28/48 sind in `lv_conf.h` abgeschaltet.
- Neue Schriftgröße oder weitere Zeichen: Font mit `lv_font_conv` (npm) neu erzeugen, Quellen liegen in `.pio/libdeps/waage/lvgl/scripts/built_in_font/`. Beispiel für 20 px:
  `lv_font_conv --no-compress --no-prefilter --bpp 4 --size 20 --font Montserrat-Medium.ttf -r 0x20-0x7F,0xA0-0xFF,0x2022 --font FontAwesome5-Solid+Brands+Regular.woff -r <Symbolcodes aus built_in_font_gen.py> --format lvgl --force-fast-kern-format --lv-font-name lv_font_app_20 -o src/ui/fonts/lv_font_app_20.c`
  Danach in `ui/fonts/fonts.h` deklarieren und in `ui/theme.cpp` (`fontFor`) eintragen.
- Texte: `src/i18n/i18n.cpp` (Deutsch/Englisch, UTF-8). Neue Texte als Eintrag in `Str` und in die Tabelle aufnehmen.
- Achtung: `uploadfs` überschreibt das ganze LittleFS, also auch die Offline-Warteschlange (`/queue.jsonl`). Vorher prüfen, ob Updates ausstehen.
- Pins für Display/Touch stehen in `include/pins.h` (aus dem Waveshare-Demo übernommen). Der Touch wird direkt über `Wire` gelesen, nicht über den LovyanGFX-Touchtreiber, weil der Bus per Mutex (`bus/i2c_bus`) geschützt ist.
- Panel-Feineinstellung: `display.cpp` überschreibt nach dem Init einige ST7789-Register (`lcdTunePanel`). Mit den LovyanGFX-Defaults (VCOMS 0x28) zeigte das Panel Geisterbilder auf dunklen Flächen, mit dem Demo-Wert 0x19 waren sie dunkler statt heller. Per Sichtprüfung am Gerät (Menü Einstellungen → Display) wurde `0x24` (36) als bester Wert gewählt, er ist der Standard `DISPLAY_VCOMS_DEFAULT`. Ein gespeicherter NVS-Wert hat Vorrang vor dem Standard. LCM-Control muss `0x0C` bleiben, `0x2C` aus dem Demo vertauscht Rot und Blau. Diese Werte nicht ohne Sichtprüfung ändern.
- NFC: `nfc/ndef.cpp` (NDEF-TLV, reine Funktionen, am Rechner mit g++ getestet), `nfc/tags.cpp` (Spule/Box/Benutzer/Admin), `nfc/nfc.cpp` (PN532-Task per Software-SPI, ca. 4 Hz). Antwortet das Modul nicht, wird die Initialisierung alle 15 s wiederholt (Log: `[NFC] Firmware-Abfrage fehlgeschlagen`). Menü → NFC zeigt den aufliegenden Tag und schreibt Box-Tags. Die Meldungen `pinMode(): Invalid IO 255` beim Start stammen von der Adafruit-Bibliothek (kein IRQ/Reset-Pin) und sind harmlos.
- Schreibschutz-Prüfung vor dem Schreiben: CC-Schreibzugriff, Lock-Bytes in Page 2 (Pages 4 bis 15) und dynamische Lock-Bytes (NTAG215 Page 0x82, NTAG216 Page 0xE2). Nach dem Schreiben wird zurückgelesen und verglichen.

- Filament-Tags: Menü → NFC → Filament → Typ → Hersteller → Schreiben. Typen (mit Temperaturbereich) und Hersteller stehen im Katalog (`storage/catalog.cpp`, NVS, überlebt `uploadfs`) und werden über die Weboberfläche gepflegt (`/api/catalog`, max. 24 Einträge je Liste, Typname max. 15, Herstellername max. 31 Zeichen). Standard: PLA 190–220, ABS 230–260, PETG 220–250; Hersteller Elegoo, Generic. `color_hex` ist vorerst fest `FFFFFF` (TODO: Farbwahl).
- Benutzer (Besitzer): Die Webseite legt Benutzer an (`storage/users.cpp`, NVS, max. 16, ID fortlaufend) und löst das Schreiben des Benutzer-Tags aus. Tag-Inhalt: `{"type":"user","id":3,"name":"Fabian","token":"<32 Hex>"}`. Ein Benutzer-Tag an der Waage meldet an (`app/session.cpp`), maßgeblich sind ID und Token. Ein gelöschter Benutzer wird abgemeldet und sein Tag ist ungültig. Abmeldung nach `USER_TIMEOUT_MS` (15 min) ohne Spulen-/Box-Scan oder per Tippen auf den Namen in der Statusleiste von Home.
- **Token pro Benutzer:** Die ID allein meldet nicht an, sonst könnte jeder per Handy-App einen Tag mit `"id":1` schreiben. „Tag schreiben“ erzeugt ein neues geheimes Token (32 Hex, gespeichert mit `usersSetTagToken` sobald der Schreibauftrag angenommen ist, NVS-Schlüssel `tok<id>` im Namespace `users`), das auf den Tag kommt; es gilt nur der zuletzt geschriebene Tag (wie beim Admin-Tag). Prüfung mit `tagCheckUserToken` (`nfc/tags`, nativ getestet, Vergleich ohne frühen Abbruch). Der Inhalt von Benutzer-Tags geht wie beim Admin-Tag nicht an `/api/status`. Ein Tag lässt sich weiterhin 1:1 kopieren; das Token verhindert nur selbst geschriebene Tags.
- **Migration:** Tags aus älteren Versionen haben kein Token und melden nicht mehr an. Das Gerät zeigt „Tag veraltet, im Web neu schreiben“, die Webseite markiert „Tag schreiben“ mit ⚠, solange für den Benutzer noch kein Tag mit Token geschrieben wurde (`"tag":false` in `/api/users`). Am Gerät nicht geprüft.
- Web-Schreibaufträge (`NfcWriteOrigin::Web`) schreiben nur auf einen **neu aufgelegten** Tag (Tag erst entfernen, dann auflegen) und überschreiben keine Spulen-Tags (Status `occupied`). Geräte-Aufträge (Menü → NFC) schreiben sofort.
- Spoolman-Anbindung (offen, Schritt 5): Der angemeldete Benutzer wird als `owner` an die Spule geschrieben. Dafür in Spoolman unter Einstellungen → Extra Fields → Spool ein Textfeld `owner` anlegen. Wert JSON-kodiert wie `nfc_uid`: `{"extra": {"owner": "\"Fabian\""}}`. Ohne angemeldeten Benutzer bleibt das Feld unverändert.

### 7.4 Stand der Spoolman-Anbindung (geprüft gegen Spoolman 0.27.0 unter http://spoolman.local:7912)
- Spulen werden über die **nativen NFC-Tags** gefunden: `GET /api/v1/spool?tag=<UID>&allow_archived=true&limit=1` (Antwort `[]` = unbekannt). Verknüpfen: `POST /api/v1/spool/{id}/tag` mit `{"uid":"...","format":"ntag"}` (409 = Tag gehört schon zu einer anderen Spule). Die UID wird von Spoolman normalisiert (Großbuchstaben, ohne Trennzeichen). Das alte Konzept mit Extra-Feld `nfc_uid` und clientseitiger Suche in der Spulenliste entfällt.
- Gewicht schreiben: `PATCH /api/v1/spool/{id}` mit `remaining_weight`. Laut API ist `remaining_weight` nur erlaubt, wenn die Filamentsorte ein Füllgewicht (`filament.weight`) hat, sonst kommt ein Fehler (der Client gibt den Text aus). Alternativ gibt es `PUT /api/v1/spool/{id}/measure` mit dem Bruttogewicht (`weight`), das Spoolman über das Leerspulengewicht verrechnet.
- **Achtung Extra-Felder:** Ein `PATCH` mit `extra` ersetzt **alle** Extra-Felder der Spule. Der Client liest deshalb vorher die Spule und führt die Felder zusammen. Werte sind JSON-kodierte Strings (`"\"Fabian\""`).
- Fehlertexte kommen als `{"message": "..."}` (404 bei unbekannter ID).
- Der Client (`net/spoolman.cpp`) läuft in einem eigenen Task mit Auftragswarteschlange, damit blockierende HTTP-Anfragen die Weboberfläche und OTA nicht aufhalten. URL: `SPOOLMAN_URL` in `secrets.h`, änderbar auf der Webseite (NVS). Alle 30 s Erreichbarkeitsprüfung (`/api/v1/info`).
- Bei jedem erkannten Spulen-Tag (oder Tag ohne bekanntes Format) wird die Spule nachgeschlagen (`app/session.cpp`). Anzeige: Menü → NFC, Webseite → Spoolman.
- **Am Gerät geprüft** (Testlauf mit Testdaten, danach gelöscht, 22 von 22 Prüfungen): Lookup per ID und per UID (Hersteller, Material, Restgewicht, Leerspulengewicht vom Hersteller, Füllgewicht, Ort, Owner), Tag verknüpfen inkl. 409-Konflikt, Update von Gewicht/Ort/Owner (Feld `owner` wird angelegt, andere Extra-Felder bleiben erhalten), Umlaute im Owner, Fehlerfall Sorte ohne Füllgewicht (HTTP 400 mit allgemeiner Meldung von Spoolman).
- Debug-Endpunkte (mit Passwort): `POST /api/spoolman/lookup?uid=..&id=..`, `/api/spoolman/update?id=..&remaining=..&location=..&owner=..`, `/api/spoolman/link?id=..&uid=..`. Ergebnis über `GET /api/spoolman` (`lookup`, `write`).
- Über OTA aufspielen: `rm -rf .pio/build/waage-ota` hilft, falls der OTA-Build wegen eines beschädigten Build-Verzeichnisses abbricht.


## 14. Weitere Funktionen (Stand Oktober 2026)
- **Tests:** `pio test -e native` (Unity, auf dem Rechner) prüft `nfc/ndef`, `nfc/tags` und `core/queue_core` (18 Tests). Neue Logik ohne Hardwarezugriff möglichst in eine Datei ohne Arduino-Abhängigkeit legen und dort testen, die Quellen stehen in `build_src_filter` des Envs `native`.
- **Katalogabgleich mit Spoolman** (`doCatalogSync` in `net/spoolman.cpp`, `catalogMerge` in `storage/catalog.cpp`): Neue **Hersteller** (`GET /vendor`) und **Materialien** (aus `GET /filament`, Feld `material`) aus Spoolman werden in den Katalog **ergänzt**; vorhandene Einträge bleiben unverändert, es wird nichts gelöscht (Name ohne Beachtung der Groß-/Kleinschreibung). Läuft automatisch alle `CATALOG_SYNC_MS` (10 min), sobald Spoolman erreichbar ist, und auf Knopfdruck (Webseite → Filament-Katalog → „Jetzt abgleichen“, `POST /api/catalog/sync`, Ergebnis `GET /api/catalog/sync`). Der Temperaturbereich eines neuen Typs: kleinster und größter `settings_extruder_temp` der Filamente dieses Materials; bei nur einem Wert +/- 10 °C (Annahme), ohne Wert übliche Werte (PLA 190–220, PETG 220–250, ABS 230–260, ASA 240–260, TPU 210–230, PA 250–280, PC 260–300, sonst 190–230). Nicht übernommen werden Namen, die nicht passen (Typ über 15, Hersteller über 31 Zeichen) oder wenn die Liste voll ist (24). Farben werden nicht abgeglichen. Am Gerät mit leerer und mit gefüllter Spoolman-Instanz geprüft; der Spoolman-Task hat 16 KB Stack (mit 10 KB stürzte der Abgleich beim Speichern ab, `Catalog` liegt im Merge auf dem Heap).
- **Filament-Farben:** Der Katalog hat zusätzlich `colors` (Name + Hex, max. 24, über die Webseite pflegbar). Ablauf am Gerät: Typ → Hersteller → Farbe → Schreiben. Der Tag enthält `color_hex` aus der Auswahl.
- **Unbekannte Spule** (`screenAssignSpoolOpen`): Erkennt die Waage einen Spulen-Tag (oder einen Tag ohne bekanntes Format), den Spoolman nicht kennt, öffnet sich auf dem Startbildschirm automatisch die Auswahl aus der Spoolman-Spulenliste (max. 60, `GET /spool?limit=60`, gefiltert und gestreamt mit HTTP/1.0). Danach: Tag verknüpfen (`POST /spool/{id}/tag`), Spule nachschlagen, Tag mit `spoolman_id` beschreiben (vorhandene OpenSpool-Daten bleiben erhalten). Auch über Menü → NFC → „Spule zuordnen“ erreichbar, solange ein unbekannter Tag aufliegt.
- **Offline-Warteschlange** (`core/queue_core`, `storage/queue`): Updates, die wegen Netzwerkfehler oder HTTP 5xx scheitern, landen in `/queue.jsonl` (je Spule nur das neueste, max. 100) und werden automatisch nachgeholt, sobald Spoolman erreichbar ist (alle 15 s). Dauerhafte Fehler (HTTP 4xx) werden verworfen und gezählt. `SpoolmanResult.queued` zeigt, dass ein Update eingereiht wurde (zählt für den Ablauf als erledigt). Zeitstempel per NTP (`Europe/Berlin`), 0 ohne Uhr. Home zeigt „Ausstehende Updates: n“, die Webseite (Spoolman) bietet „Jetzt senden“ und „Alle verwerfen“. **`uploadfs` löscht die Warteschlange.**
- **WLAN-Einrichtung (wie bei Tasmota):** Die Zugangsdaten stehen nur im NVS (Webseite → WLAN oder serielle Konsole). Die Werte `WIFI_SSID`/`WIFI_PASSWORD` aus `secrets.h` werden **einmalig** beim ersten Start übernommen (Platzhalter `dein-wlan` zählt nicht), danach nicht mehr. Sind keine Daten gespeichert, startet **sofort** der Hotspot `Filamentwaage-Setup` (Passwort = `OTA_PASSWORD`, falls mindestens 8 Zeichen, sonst offen) und Home zeigt „WLAN einrichten: …“. Ein Captive Portal leitet die Prüf-URLs der Betriebssysteme und jeden fremden Host auf `http://192.168.4.1/#wifi` (Wildcard-DNS im Hotspot, `net/webserver.cpp`, `captiveRedirect`). Kann sich das Gerät mit gespeicherten Daten eine Minute lang nicht verbinden, startet der Hotspot ebenfalls und läuft parallel zu den Verbindungsversuchen. Er endet, sobald das Gerät eine Minute stabil verbunden ist und niemand daran hängt. Wer im Hotspot ist (Verbindung über die Hotspot-Schnittstelle), muss sich auf der Webseite nicht noch einmal anmelden.
- **Serielle Notbedienung** (USB, 115200 Baud, Zeilenende `\n`): `wifi set <SSID>|<Passwort>`, `wifi reset`, `wifi status`, `admin reset`, `reboot`. Damit lässt sich das WLAN auch ohne Netz neu setzen, und `admin reset` stellt das Admin-Passwort `Filament`, die Geräte-PIN `1234`, die Sperre und einen ungültigen Admin-Tag wieder her (siehe Abschnitt 15).
- **Nicht am Gerät geprüft:** der Start des Hotspots beim Boot ohne gespeicherte Daten (der Hotspot lässt sich über die Webseite ein- und ausschalten und Captive-DNS/Weiterleitung sind geprüft), das Beitreten mit einem Handy und der automatische Rückfall nach einer Minute ohne Verbindung.
- **HX711 ohne Daten:** Ohne angeschlossenen HX711 liest die Bibliothek am freien DOUT-Pin lauter Nullen (0) oder Einsen (-1). Mit dem gespeicherten Offset ergibt das eine feste Anzeige wie -67 g. Der `scale`-Task verwirft deshalb die Rohwerte 0, -1 und die Sättigungswerte (`rawInvalid` in `scale/scale.cpp`) und meldet alle 5 s, wie viele es waren (`[SCALE] n ungültige Lesungen verworfen`). Treten diese Meldungen mit angeschlossenem HX711 auf, ist die Versorgung oder das SCK-Timing gestört (Kondensatoren 100 µF und 100 nF am HX711 helfen).
- **Log:** `util/log` schreibt alle Ausgaben (`logPrintf`/`logPrintln` statt `Serial`) und die ESP-IDF-Meldungen in einen Ringpuffer (160 Zeilen, PSRAM). Webseite → Log (`GET /api/log?since=N`, Header `X-Log-Seq`). Arduino-eigene `log_e`-Meldungen des Cores erscheinen dort nicht.
- **Bildschirmschoner:** Nach `dim` Minuten ohne Berührung, Tag-Scan, Gewichtsänderung (> 3 g) oder Meldung wird auf 10 % gedimmt, nach `off` Minuten ausgeschaltet (Standard 5/15, 0 = nie, NVS, Webseite → System). Der erste Tipp weckt nur und löst nichts aus.
- **Webseite:** Menü (Übersicht, Kalibrierung, Benutzer, Filament-Katalog, Spoolman, WLAN, Log, System), zweisprachig (folgt der Gerätesprache, Texte in `I18N` in `net/web_page.h`, neue Texte dort in beiden Sprachen ergänzen). Die Seite verlangt zuerst eine Anmeldung (siehe Abschnitt 15).

## 15. Admin, Sperre und Sprache pro Benutzer
- **Admin** (`storage/admin.cpp`, NVS): Der Admin darf alle Einstellungen am Gerät und auf der Webseite ändern, Benutzer anlegen und die Sperre abschalten. Normale Benutzer dürfen nur wiegen (Spule/Box, Spoolman-Update) und ihre eigene Sprache wählen. Der Admin kann einzelnen Benutzern das **Tara-Recht** und das Recht **Filament** (Typen, Hersteller und Farben im Katalog pflegen, `User.canCatalog`, `POST /api/users/catalog?id=&allow=1|0`) geben (`User.canTare`, Häkchen „Tara“ in der Benutzerliste der Webseite, `POST /api/users/tare?id=&allow=1|0`). Benutzer legen sich nicht selbst an: das geht nur mit Admin-Rechten (Webseite).
- **Passwort:** am Anfang `Filament`, vom Admin auf der Webseite änderbar (4 bis 32 Zeichen, kein `:`). Gespeichert nur als Salted-SHA-256-Hash. Die Webseite warnt, solange das Standardpasswort aktiv ist. **Notfall:** serieller Befehl `admin reset` (Passwort `Filament`, Sperre an, Admin-Tag ungültig).
- **Sperre** (`adminLockEnabled`, am Anfang **an**): Der Admin kann sie auf der Webseite (Benutzer → Admin) abschalten, falls der Inhaber sie nicht möchte. Ist sie aus, darf jeder am Gerät und im Netzwerk alles ändern.
- **Webseite:** Die Seite zeigt zuerst eine **Login-Maske** (Name + PIN bzw. Passwort). Sitzungen liegen im RAM (`net/websession.cpp`, Cookie `sid`, HttpOnly, SameSite=Strict, `WEB_SESSION_MS` 30 min ohne Anfrage, max. 6). Drei Rollen (`requireRole` in `net/webserver.cpp`):
  - **Admin** (Name `admin`, Admin-Passwort): sieht und darf alles. Für Skripte geht zusätzlich Basic-Auth (`curl -u admin:...`).
  - **Benutzer** (Name + eigene **PIN**, 4 bis 6 Ziffern): schlichte Ansicht mit Gewicht, NFC, WLAN-Status (verbunden ja/nein), Tara-Knopf nur mit Tara-Recht, eigene Sprache und eigene PIN ändern. Kein Log, keine Einstellungen, keine Benutzerliste (alle anderen Schnittstellen antworten 403). Die PIN setzt der Admin beim Anlegen oder später (`POST /api/users/pin?id=&pin=`, leer = entfernen); ein Benutzer ohne PIN kann sich nur per NFC am Gerät anmelden, nicht auf der Webseite. Der Benutzer ändert seine PIN mit der alten PIN (`POST /api/me/pin`).
  - Ein Benutzer mit dem Recht **Filament** sieht zusätzlich die Seite **Filament-Katalog** (`GET/POST /api/catalog`, `/api/catalog/sync`), sonst nichts.
  - **Generic** (Name `generic`, am Anfang ohne PIN, vom Admin abschaltbar oder mit PIN schützbar): nur wiegen und NFC lesen, kein Tara, keine Sprache.
  Die PINs liegen nur als Salted-SHA-256-Hash im NVS (`pin<ID>`, ID 0 = Generic). Eine geänderte PIN, ein gelöschter Benutzer oder ein geändertes Admin-Passwort beenden die betroffenen Sitzungen. Der Status (`/api/status`) enthält für Nicht-Admins keine Gerätedaten (IP, Speicher, Firmware) und keinen Inhalt eines Admin-Tags. Ist die Sperre aus, ist jeder Admin. 5 Fehlversuche (Login oder Basic-Auth, gemeinsam gezählt) sperren die Anmeldung 30 s. `OTA_PASSWORD` gilt nur noch für das Flashen per WLAN und für das Setup-WLAN. Wer im **geschützten** Setup-WLAN ist (OTA_PASSWORD mindestens 8 Zeichen), braucht keine zweite Anmeldung. Ein offener Hotspot zählt nicht als Ausweis.
- **Geräte-PIN:** Die Bildschirmtastatur ist auf dem kleinen Display nicht nutzbar. Am Gerät meldet sich der Admin deshalb mit einer eigenen **Geräte-PIN** (4 bis 6 Ziffern, Ziffernblock am Bildschirm) oder dem Admin-Tag an. Am Anfang `1234` (`ADMIN_DEFAULT_PIN`), der Admin ändert sie auf der Webseite (Benutzer → Admin, `POST /api/admin/pin`, Salted-SHA-256 im NVS); die Seite warnt, solange die Standard-PIN aktiv ist. Das Passwort gilt nur noch für die Webseite. Nach 5 falschen Eingaben ist die Eingabe 30 s gesperrt. `admin reset` setzt auch die Geräte-PIN zurück.
- **Gerät:** Das Menü ist für alle offen. **Einstellungen** und **NFC** verlangen den Admin (`uiAdminGate`), **Tara** den Admin oder einen angemeldeten Benutzer mit Tara-Recht (`sessionUserMayTare`). Anmeldung mit der **Geräte-PIN** (Ziffernblock) oder durch Auflegen des **Admin-Tags**. Die Anmeldung endet nach `ADMIN_TIMEOUT_MS` (5 min) ohne Bedienung oder über „Admin abmelden“ im Menü bzw. Tippen auf „Admin“ in der Statusleiste. Danach springt die Anzeige zu Home zurück.
- **Admin-Tag:** `{"type":"admin","token":"<32 Hex>"}`. Die Webseite erzeugt ihn (Benutzer → Admin → Admin-Tag schreiben); es gilt immer nur der zuletzt erzeugte Token. Ein NFC-Tag lässt sich kopieren, das Passwort ist sicherer.
- **Sprache pro Benutzer:** `User.lang` (0 = Gerätesprache, 1 = Deutsch, 2 = Englisch, im Benutzer-JSON im NVS). Beim Anmelden per Tag wird sie geladen (`i18nApply`, speichert nichts), beim Abmelden kommt die Gerätesprache zurück (`i18nRestoreDefault`). Der Benutzer ändert sie am Gerät unter Menü → Meine Sprache, der Admin auch auf der Webseite. Menü → Einstellungen → Sprache und die Webseite stellen die **Gerätesprache** ein (`i18nDefault`).
- **Am Gerät per HTTP geprüft** (curl gegen das laufende Gerät): Login der drei Rollen, Rechte-Matrix (Benutzer/Generic bekommen für Log, Benutzerliste, WLAN, Spoolman, Theme, Admin, Neustart 403), Tara-Recht, eigene Sprache und PIN, Beenden der Sitzungen, Sperre aus/an, Fehlversuch-Sperre.
- **Noch nicht geprüft:** das Aussehen der neuen Login-Maske und der Benutzer-Ansicht im Browser (nur die JavaScript-Syntax mit `node --check`), Touchscreen-Anmeldung mit dem Ziffernblock (Layout und Tastenhöhe auf dem 240×320-Display), Auflegen eines Admin-Tags, Sprachwechsel beim Anmelden am Gerät, Ablauf der Admin-Anmeldung.

## 16. Wiegeablauf (Differenzwägung)
- **Logik:** `core/weigh_core.{h,cpp}` (reiner Zustandsautomat `WeighFlow` plus `weighCompute`, 6 Tests in `test/test_weigh`). **Screens und Anbindung:** `ui/screen_weigh.cpp`. Zustände: Spule erkannt → Box auflegen → Spule in die Box → Bestätigen → Speichern → Fertig („Box entnehmen“), dazu Fehlerzustand mit [Nochmal]. Schwellen in `config.h` (`WEIGH_*`): „hinzugefügt“ 50 g, Timeout 120 s je Wartezustand, Mindestanzeige „Spule erkannt“ 2 s.
- **Start:** Auf Home liegt ein Spulen-Tag (oder ein Tag ohne bekanntes Format) auf, den Spoolman kennt (`ui.cpp`, `refreshTimerCb`). Unbekannte Tags öffnen weiter „Unbekannte Spule“. Bis die Waage leer und stabil ist, zeigt der Screen „Waage leeren“.
- **Box:** W1 wird eingefroren, sobald das Gewicht stabil ≥ 50 g ist **und** ein Box-Tag gelesen wird. Der `name` des Box-Tags wird als `location` an Spoolman geschickt. Danach werden nur noch Gewichte ausgewertet (Spulen-Tags ignoriert).
- **Rechnung:** Rest = (W2 − W1) − Leerspule; Leerspule aus Spoolman (Spule, Filament, Hersteller). Fehlt sie, erscheint auf „Bestätigen“ eine Eingabe mit −10/+10 (Startwert `WEIGH_DEFAULT_EMPTY_G` 250 g). Rest < −20 g: Fehler, [Speichern] ausgeblendet. −20 g bis 0: auf 0 gesetzt, Hinweis. Rest > Anfangsgewicht + 50 g: Warnung (Speichern bleibt möglich).
- **Speichern:** `spoolmanRequestUpdate(id, rest, box, owner)`; Erfolg (2xx) oder Warteschlange gilt als erledigt („Gespeichert“ bzw. „In Warteschlange“). Abgebrochen wird mit [Abbrechen] (in „Bestätigen“ das ✕), während des Sendens ist Abbrechen nicht möglich.
- **Sicherheitsnetz:** `uiShowHome()` bricht einen laufenden Ablauf ab (`weighFlowAbort`), damit keine Widgets mehr benutzt werden, die gelöscht wurden. Admin-Ablauf und Sprachwechsel springen während des Ablaufs nicht zu Home.
- **Nicht umgesetzt:** Screen „Neue Box anlegen“ (leerer oder unbekannter Box-Tag), Längenanzeige in m. **Am Gerät nicht geprüft:** die Firmware baut, die Logik ist am Rechner getestet und die Screens sind aus dem echten UI-Code gerendert, der Ablauf mit echter Waage, Box-Tag und Spoolman wurde nicht ausprobiert.

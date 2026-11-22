# CLAUDE.md – Smarte Filamentwaage mit NFC-Box-Zuordnung und Spoolman

Verbindliche Spezifikation und Entwicklernotizen der Firmware. Arbeitsweise, Sprache auf GitHub und Releases: siehe [`../CLAUDE.md`](../CLAUDE.md).

## 1. Ziel

Eine ESP32-S3-basierte Waage, die:
1. eine Filamentspule per NFC-Tag (OpenSpool-Format) identifiziert,
2. eine Lagerbox (Müslibox) per NFC-Tag identifiziert,
3. das Brutto-Gewicht der Spule per **Differenzwägung** ermittelt (Box vorher/nachher),
4. das Filament-Restgewicht berechnet und
5. in **Spoolman** `remaining_weight` und `location` (= Box) der Spule aktualisiert.

Daneben gibt es die Betriebsart **Einfache Waage** (5.4): nur wiegen, ohne Spoolman, Anmeldung und NFC.

---

## 2. Hardware

| Komponente | Details |
|---|---|
| MCU/Display | Waveshare ESP32-S3-Touch-LCD-2 (ESP32-S3R8, 8 MB Octal-PSRAM, 16 MB Flash), ST7789T3 (SPI), 240×320 Hochformat, Touch CST816 (I²C, nur Single-Touch) |
| Wägezelle | Biegebalken **5 kg**, 4 Leiter (E+, E−, A+, A−) |
| ADC | HX711-Modul (grün), versorgt mit **3,3 V** (DOUT darf keine 5 V führen), RATE = 10 Hz. Wahlweise NAU7802 oder ADS1232 (6.4) |
| NFC | PN532, **SPI-Modus** (DIP: SW1=OFF, SW2=ON), 3,3 V, Software-SPI an eigenen Pins. Der I²C-Modus lief am Waagen-Board nicht: SCL blieb dort an allen getesteten Pins (IO2/4/6/13/15 und IO47/48) dauerhaft auf low, am NodeMCU-ESP32 lief dasselbe Modul fehlerfrei. Ursache nicht geklärt. |
| Tags | **NTAG216** (vorhanden); NTAG215 muss ebenfalls unterstützt werden |
| Speicher | **keine SD-Karte**, nur Flash (NVS + LittleFS) |

### 2.1 Mechanik (3D-gedruckt, für die Software relevant)
- Biegebalken: Sockel an einem Ende, Wiegeplatte am anderen. Distanzstücke sorgen dafür, dass nur die Enden Kontakt haben. Pfeil auf der Zelle = Lastrichtung.
- Ballast (Stahl) nur im Sockel und am Rand. Die Wiegeplatte enthält keinen Ballast, ihr Eigengewicht wird tariert.
- **Ein** PN532 sitzt mittig unter der Wiegeplatte (Kunststoff, möglichst ≤ 10 mm). Um die Antenne herum ist mindestens 3 cm frei von Metall. Realistische Leseweite: 2–4 cm.
- Box-Tags kleben **mittig auf dem Boxboden** und werden gelesen, wenn die Box auf der Platte steht.
- Spulen-Tag wird gelesen, indem die Spule kurz an die markierte Scan-Zone der Platte gehalten wird.
- Folge: Während eines Ablaufs können mehrere Tags in Reichweite sein. Die Software ordnet Tags anhand ihres Typs und des aktuellen Zustands zu und ignoriert unerwartete Tags.

### 2.2 Pinbelegung
- Alle Pins zentral in `include/pins.h`, nirgends sonst hart codieren.
- Display/Touch: aus dem Waveshare-Demo für ESP32-S3-Touch-LCD-2 übernommen (`include/pins.h`). Der Touch bleibt allein auf seinem I²C-Bus (GPIO47/48).
- Wägesensor: Typ und Pins sind auf der Webseite wählbar (6.4), `pins.h` enthält nur die Standardbelegung.
- HX711 (kein I²C, eigene 2-Draht-Schnittstelle): SCK = GPIO17 (Header P1 Pin 5), DOUT = GPIO18 (Header P1 Pin 6). Versorgung 3,3 V = Header P2 Pin 1. Quelle: Schaltplan des Boards.
- PN532 (**SPI**, Software-SPI): SCK = GPIO15, MISO = GPIO13, MOSI = GPIO11, SS = GPIO12 (Header P2, Bild-Nr. 7 bis 10 im Waveshare-Pinout), 3V3 = P2 Pin 1, GND = P2 Pin 2. TODO: Ob GPIO11 bis 15 sonst belegt sind (vermutlich SD-Slot, ungenutzt), ist nicht am Schaltplan geprüft.
- Header-Belegung des Boards (Quelle: Schaltplan): P1 = IO2, IO4, IO6, IO16, IO17, IO18, IO21, IO8, IO7, IO10, IO20, IO19, GND, 5V (Pin 1–14). P2 = 3V3, GND, IO43, IO44, IO47, IO48, IO15, IO13, IO11, IO12, IO14, IO9, GND, VBAT (Pin 1–14). IO19/IO20 sind USB und nicht verwendbar.
- Verdrahtungsplan: `docs/bauen.html` (erzeugt mit `tools/wiring_svg.py`).

---

## 3. Software-Stack

- PlatformIO, Plattform **pioarduino**. Maßgeblich ist `platformio.ini` mit den Envs `waage` (USB), `waage-ota` (WLAN) und `native` (Unit-Tests am Rechner).
- Plattform und alle Bibliotheken sind **fest gepinnt**: pioarduino **55.03.312** (Arduino 3.3.12, ESP-IDF 5.5, GCC 14.2), LVGL 9.5.0, LovyanGFX 1.3.0, HX711 0.7.5, Adafruit PN532 1.3.4, Adafruit BusIO 1.17.4, ArduinoJson 7.4.3. Nicht `stable` verwenden: Am 21.11.2026 sprang `stable` auf 61.4.0 (Arduino 4.0.0, GCC 15.2), damit scheitert LVGL 9.5.0 an `lv_blend_helium.S`. Plattform- oder Bibliotheks-Updates nur bewusst, als eigene Aufgabe und mit grünem CI-Build.
- **LVGL 9.5.0:** `include/lv_conf.h` stammt aus dem 9.5.0-Template, keine 8.x-Konfiguration verwenden. Nicht benötigte Features (OpenGL, 3D, Blur) bleiben deaktiviert.
- **LovyanGFX** als Display-Treiber (nicht TFT_eSPI). Die LVGL-9-Anbindung (Flush- und Touch-Callback) ist selbst geschrieben. Aus den Waveshare-Beispielen werden nur Pins und Treiber-Init übernommen.
- Die UI ist **handgeschrieben in LVGL-C++**. EEZ Studio dient nur zum Ausprobieren von Layouts, generierter Code wird nicht übernommen.
- **NFC/NDEF:** Adafruit_PN532 für Low-Level-Zugriff (`ntag2xx_ReadPage` / `ntag2xx_WritePage`). NDEF-TLV-Parsing und -Schreiben für einen einzelnen MIME-Record sind **selbst implementiert** (`nfc/ndef.cpp`). Die NDEF-Library von Don Coleman wird nicht verwendet, weil sie für die Seeed-PN532-Library gebaut ist.
- HTTP: `HTTPClient` + `WiFiClient`; JSON: ArduinoJson 7.
- Persistenz: `Preferences` (NVS) für Konfiguration, Kalibrierung, Benutzer und Katalog; LittleFS für `theme.json` und die Offline-Warteschlange.

---

## 4. NFC-Tags

### 4.1 Allgemein
- Typ: NTAG215 oder NTAG216 (NFC Forum Type 2, ISO14443A). Ein NDEF-Record, MIME-Type `application/json`. Spulen auch als OpenPrintTag (`application/vnd.openprinttag`, siehe 4.7). Beim Lesen wird der erste passende MIME-Record einer Nachricht genommen (`ndefFindMimeRecord`), davor stehende Records (z. B. URI) werden übersprungen.
- **Kapazität nicht hart codieren**, sondern aus dem Capability Container lesen (Page 3, Byte 2, Wert × 8 = NDEF-Bereich in Byte):

| Tag | CC Byte 2 | NDEF-Bereich | User-Pages |
|---|---|---|---|
| NTAG215 | `0x3E` | 496 Byte | 4 – 129 |
| NTAG216 | `0x6D` | 872 Byte | 4 – 225 |

- NDEF-TLV ab Page 4: `0x03`, Länge (1 Byte, bzw. `0xFF` + 2 Byte bei ≥ 255), Record, danach `0xFE` (Terminator).
- Record: Short Record (SR) bei Payload < 256 Byte, TNF = `0x02` (MIME), Type = `application/json`.
- Vor dem Schreiben prüfen: Record + TLV + Terminator müssen in die Kapazität passen. Schreibschutz-Prüfung: CC-Schreibzugriff, Lock-Bytes in Page 2 (Pages 4 bis 15) und dynamische Lock-Bytes (NTAG215 Page 0x82, NTAG216 Page 0xE2). Ist ein Tag gesperrt, gibt es eine Fehlermeldung und es wird nicht geschrieben. Nach dem Schreiben wird zurückgelesen und verglichen.
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
  2. Sonst über die Tag-**UID** (native NFC-Tags von Spoolman, siehe 7.2).
  3. Sonst → „Unbekannte Spule“ (siehe 7.4).

### 4.3 Box-Tag (eigenes Format)
```json
{ "type": "box", "id": "BOX-03", "name": "Box 03" }
```
- `name` wird 1:1 als Spoolman-`location` verwendet.
- Leerer bzw. unbekannter Tag beim Box-Schritt → „Neue Box anlegen“ (siehe 5.3).

### 4.4 Benutzer- und Admin-Tag
- Benutzer: `{"type":"user","id":3,"name":"Fabian","token":"<32 Hex>"}` (siehe 11.1). Ältere Tags ohne `token` melden nicht mehr an.
- Admin: `{"type":"admin","token":"<32 Hex>"}` (siehe 11.2).

### 4.5 Tag-Typ-Erkennung
- JSON enthält `"protocol": "openspool"` → Spule
- JSON enthält `"type": "box"` / `"user"` / `"admin"` → Box / Benutzer / Admin
- MIME-Record `application/vnd.openprinttag` mit gültigem Meta- und Hauptbereich → Spule (`TagContent.openPrintTag`). Hat der Tag auch einen JSON-Record, gilt der JSON-Record.
- Kein NDEF / kein gültiges JSON → „leer/unbekannt“

### 4.6 Umsetzung
- `nfc/ndef.cpp` (NDEF-TLV, reine Funktionen, nativ getestet), `nfc/cbor.cpp` und `nfc/openprinttag.cpp` (OpenPrintTag, nativ getestet), `nfc/tags.cpp` (Spule/Box/Benutzer/Admin parsen und erzeugen), `nfc/nfc.cpp` (PN532-Task per Software-SPI, ca. 4 Hz).
- Antwortet das Modul nicht, wird die Initialisierung alle 15 s wiederholt (Log: `[NFC] Firmware-Abfrage fehlgeschlagen`). Die Meldungen `pinMode(): Invalid IO 255` beim Start stammen von der Adafruit-Bibliothek (kein IRQ/Reset-Pin) und sind harmlos.
- Menü → NFC zeigt den aufliegenden Tag und schreibt Box- und Filament-Tags.
- **Filament-Tags:** Menü → NFC → Filament → Typ → Hersteller → Farbe → [OpenSpool] oder [OpenPrintTag]. Der Tag enthält `color_hex` (bzw. `primary_color`) aus der Auswahl. Typen (mit Temperaturbereich), Hersteller und Farben stehen im Katalog (`storage/catalog.cpp`, NVS, überlebt `uploadfs`) und werden über die Webseite gepflegt (`/api/catalog`, je Liste max. 24 Einträge, Typname max. 15, Herstellername max. 31 Zeichen). Standard: PLA 190–220, ABS 230–260, PETG 220–250; Hersteller Elegoo, Generic.
- **Web-Schreibaufträge** (`NfcWriteOrigin::Web`) schreiben nur auf einen **neu aufgelegten** Tag (erst entfernen, dann auflegen) und überschreiben keine Spulen-Tags (Status `occupied`). Geräte-Aufträge (Menü → NFC) schreiben sofort.

### 4.7 OpenPrintTag (Issue #24)
- Offener Standard von Prusa Research (MIT-Lizenz), Spezifikation: https://specs.openprinttag.org, Quellen und Testdaten: https://github.com/prusa3d/OpenPrintTag (geprüft mit dem Stand vom 9.10.2026).
- Aufbau: NDEF-MIME-Record `application/vnd.openprinttag`, Payload = CBOR mit Integer-Schlüsseln: Meta-Bereich (Lage der Bereiche), Hauptbereich (Materialdaten), optional Aux-Bereich (Verbrauch, von Druckern beschrieben). Jeder Abschnitt höchstens 512 Byte. Unbekannte Schlüssel werden übersprungen.
- **Einschränkung:** Der Standard ist für **NFC-V** (ISO 15693, ICODE SLIX2) definiert. Der PN532 kann kein ISO 15693, echte OpenPrintTag-Spulen (z. B. Prusament) erkennt die Waage deshalb **nicht**. Die Waage liest und schreibt dasselbe Datenformat auf NTAG215/216. Solche Tags sind nicht standardkonform; ob Drucker oder Apps sie lesen, ist nicht geprüft. Für echte Tags bräuchte es einen anderen Leser (z. B. PN5180). Entscheidung von Fabian am 22.11.2026: Format auf NTAG, PN532 bleibt.
- Gelesen werden `material_class` (8), `material_type` (9, Enum, Tabelle in `openprinttag.cpp`), `material_name` (10), `brand_name` (11), Gewichte (16, 17, 18), `primary_color` (19), Drucktemperatur (34, 35) und aus dem Aux-Bereich `consumed_weight` (0). Daraus wird ein `SpoolTag` (Typ = Enum-Abkürzung, sonst `material_name`). `spoolman_id` gibt es im Format nicht, Spoolman findet die Spule über die UID.
- Geschrieben werden Meta (`aux_region_offset`, `aux_region_size`), Hauptbereich (FFF, Typ falls im Enum, Name = Typ aus dem Katalog, Hersteller, Farbe, Temperaturen) und ein leerer Aux-Bereich (32 Byte, offene Map `BF FF`, auf 4-Byte-Grenze des Tags). Nicht geschrieben: `instance_uuid` (die Spezifikation leitet sie dann aus der Tag-UID ab), Gewichte, `write_protection`.
- Ein OpenPrintTag wird beim Zuordnen einer unbekannten Spule (7.4) **nicht** mit JSON überschrieben, die Verknüpfung läuft nur über die UID. Web-Schreibaufträge überschreiben ihn nicht (`occupied`).
- Webseite: `/api/status` meldet `nfc.format` (`json`, `openprinttag` oder leer), `nfc.json` enthält bei OpenPrintTag eine Kurzbeschreibung (`optDescribeJson`).
- Nicht umgesetzt: Restgewicht bzw. `consumed_weight` nach dem Wiegen auf den Tag schreiben, `workgroup`.
- Am Gerät nicht geprüft: Lesen und Schreiben mit echtem PN532 und NTAG. Nativ getestet mit dem Beispiel-Tag der Spezifikation (`test/test_openprinttag`).

---

## 5. Wiegeablauf (Differenzwägung)

### 5.1 Zustände
```
IDLE (Home, Live-Gewicht wie eine Küchenwaage)
 └─ bekannter Spulen-Tag ─────────────► SPOOL_IDENTIFIED (Spoolman-Daten anzeigen, mind. 2 s)
SPOOL_IDENTIFIED
 └─ Waage leer & stabil ──────────────► WAIT_BOX ("Box auflegen")
WAIT_BOX
 └─ Gewicht ≥ 50 g & stabil
    UND Box-Tag gelesen ──────────────► W1 einfrieren → WAIT_SPOOL_INSERT ("Spule in die Box")
WAIT_SPOOL_INSERT
 └─ Gewicht ≥ W1 + 50 g & stabil ─────► W2 einfrieren → CONFIRM (Box, Spule, Restgewicht)
CONFIRM
 ├─ [Speichern] ──────────────────────► UPLOAD (Spoolman bzw. Warteschlange)
 └─ [Abbrechen] ──────────────────────► IDLE
UPLOAD ─ fertig ──────────────────────► DONE ("Box entnehmen") ─ Waage leer & stabil ─► IDLE
```
- Umsetzung: Logik in `core/weigh_core.{h,cpp}` (reiner Zustandsautomat `WeighFlow` plus `weighCompute`, Tests in `test/test_weigh`), Screens und Anbindung in `ui/screen_weigh.cpp`. Dazu ein Fehlerzustand mit [Nochmal] und der Zustand `NewBox`.
- Schwellen in `config.h` (`WEIGH_*`): „hinzugefügt“ 50 g, Timeout 120 s je Wartezustand (danach zurück nach `IDLE` mit Hinweis), Mindestanzeige „Spule erkannt“ 2 s.
- **Start:** Auf Home liegt ein Spulen-Tag (oder ein Tag ohne bekanntes Format), den Spoolman kennt (`ui.cpp`, `refreshTimerCb`). Unbekannte Tags öffnen „Unbekannte Spule“. Bis die Waage leer und stabil ist, zeigt der Screen „Waage leeren“.
- Ab `WAIT_BOX` werden **Spulen**-Tags ignoriert, auch der bereits gescannte. Die Box wird in `WAIT_BOX` festgelegt und danach nicht mehr geändert, danach zählen nur noch Gewichte.
- **QR-Code zur Spule (virtuelles Label):** Auf „Bestätigen“ öffnet der Button mit QR-Symbol (rechts neben dem Titel, nur wenn eine Spoolman-URL gesetzt ist) einen Vollbild-QR-Code mit dem Link `<Spoolman-URL>/spool/show/<id>` (`core/spool_url`, Test `test/test_spoolurl`) und einem ✕ zum Schließen. LVGL `lv_qrcode` ist dafür in `lv_conf.h` aktiviert (`LV_USE_QRCODE 1`). Das Symbol ist aus kleinen Quadraten gezeichnet, die Schrift hat kein QR-Zeichen. Der Link zeigt auf die Spoolman-Adresse aus den Einstellungen, das Handy muss sie erreichen können.
- **Abbrechen:** In jedem Zustand führt [Abbrechen] (in „Bestätigen“ das ✕) zu `IDLE`. Während des Sendens ist Abbrechen nicht möglich.
- **Sicherheitsnetz:** `uiShowHome()` bricht einen laufenden Ablauf ab (`weighFlowAbort`), damit keine gelöschten Widgets mehr benutzt werden. Admin-Ablauf und Sprachwechsel springen während des Ablaufs nicht zu Home.

### 5.2 Rechnung und Plausibilität
```
spool_gross      = W2 - W1
empty_spool      = spool.spool_weight
                   ?? spool.filament.spool_weight
                   ?? spool.filament.vendor.empty_spool_weight
                   ?? manuelle Eingabe auf „Bestätigen“ (−10/+10, Start WEIGH_DEFAULT_EMPTY_G = 250 g)
remaining_weight = spool_gross - empty_spool
```
- Das Boxgewicht muss **nicht** bekannt sein. Der Inhalt der Box (andere Spulen, Trockenmittel) spielt keine Rolle.
- `remaining_weight < −20 g` → Fehler „Leerspulengewicht prüfen“, [Speichern] ausgeblendet.
- `−20 g ≤ remaining_weight < 0` → auf 0 setzen, Hinweis.
- `remaining_weight > initial_weight + 50 g` (falls bekannt) → Warnung, Speichern bleibt möglich.
- Überlast: Gesamtgewicht > 4500 g (`SCALE_OVERLOAD_G`) → Warnung „Überlast“ (Zelle 5 kg).
- **Speichern:** `spoolmanRequestUpdate(id, rest, box, owner)`. Erfolg (HTTP 2xx) oder Eintrag in die Warteschlange gilt als erledigt („Gespeichert“ bzw. „In Warteschlange“).
- **Leere Spule archivieren:** Ist der Rest auf „Bestätigen“ ≤ `WEIGH_ARCHIVE_MAX_G` (10 g), erscheint ein Schalter „Archivieren: ja/nein“ (Standard nein). Mit „ja“ sendet [Speichern] zusätzlich `archived: true` im selben `PATCH` und löst danach den Tag der Spule (`DELETE /api/v1/spool/{id}/tag/{uid}`, 404 wird ignoriert, die Spule bleibt dann trotzdem archiviert). Beides steht auch in der Offline-Warteschlange (`archive`, `tag_uid`). Der Tag selbst wird nicht gelöscht, weil die Spule beim Wiegen nicht auf dem Leser liegt; er lässt sich über Menü → NFC neu beschreiben. Endpunkte laut Spoolman-Quelltext (0.27.0 und master) gelesen, am Gerät und gegen eine laufende Instanz nicht geprüft.
- **Länge:** Auf „Bestätigen“ steht unter dem Restgewicht „ca. n m Filament“, wenn Spoolman Dichte und Durchmesser der Filamentsorte kennt (`filamentLengthM` in `core/weigh_core`, nativ getestet): `laenge_m = remaining_weight / (dichte_g_cm3 * pi * (d_mm/20)^2 * 100)`. Ohne diese Angaben bleibt die Zeile leer. Screen am Gerät nicht geprüft.

### 5.3 Neue Box anlegen (Zustand `NewBox`)
- Steht in „Box auflegen“ eine Box stabil ≥ 50 g auf der Waage und liegt ein **leerer oder unbekannter** Tag auf (`tagIsBlankForBox`: kein NDEF, kein bekanntes JSON oder Box-Tag ohne `name`), öffnet sich „Neue Box anlegen“.
- Nummer mit −5/−1/+1/+5 wählen (1 bis 99), [Schreiben] schreibt `{"type":"box","id":"BOX-07","name":"Box 07"}` (`tagBuildBoxNumber`). Geschrieben wird nur, wenn noch derselbe Tag (UID) aufliegt. Danach liest das NFC-Modul den Tag neu, der Ablauf geht zurück nach „Box auflegen“ (neuer Timeout) und friert W1 wie gewohnt ein.
- Ein gültiger Box-Tag hat Vorrang. Der Spulen-Tag, der den Ablauf gestartet hat (UID), wird nie als Box angeboten. Spulen-, Benutzer- und Admin-Tags lösen den Screen nicht aus. ✕ bricht den ganzen Ablauf ab, Timeout wie in den anderen Wartezuständen. Ein eigener Schreibauftrag wird beim Verlassen abgebrochen und quittiert, damit Home wieder Tags auswertet.
- Annahme: Vorschlag für die Nummer ist 1 bzw. die nach der zuletzt geschriebenen (nur bis zum Neustart), weil es keine Liste vorhandener Boxen gibt.

### 5.4 Einfache Waage (Betriebsart und Rolle „Nur wiegen“)
- **Betriebsart** (`storage/devmode`, NVS-Namespace `devmode`, Schlüssel `simple`): „Smarte Waage“ (Standard) oder „Einfache Waage“. Umschalten nur der Admin auf der Webseite (System → Betriebsart, `POST /api/mode` mit `{"simple":true|false}`); `/api/status` meldet `simple` an alle Rollen.
- In der einfachen Waage: Benutzer-Tags melden nicht an (ein angemeldeter Benutzer wird abgemeldet), Spulen-Tags werden nicht nachgeschlagen, der Spoolman-Task ruht (keine Erreichbarkeitsprüfung, kein Katalogabgleich, Warteschlange bleibt liegen), Home zeigt „Einfache Waage“ statt „Spule scannen“ und keine ausstehenden Updates. **Tara darf jeder.** Admin-Anmeldung (PIN, Admin-Tag), Menü und Webseiten-Login bleiben unverändert.
- **Rolle „Nur wiegen“** (`User.weighOnly`, `POST /api/users/weigh?id=&allow=1|0`, Spalte in Benutzer): In der smarten Waage sieht ein so angemeldeter Benutzer nur „Filament wiegen“; seine Spulen-Tags werden nicht nachgeschlagen, der Spoolman-Ablauf startet nicht.
- `sessionSimpleWeighAllowed()` = einfache Waage oder Benutzer mit „Nur wiegen“. Dann zeigt Home den Button **Filament wiegen** → Auswahl **Nur Spule** / **Box + Spule**.
- Ablauf: Logik `core/simple_core` (`SimpleFlow`, Tests `test/test_simple`), Screens `ui/screen_simple.cpp`.
```
CLEAR (Waage leeren) ─ leer & stabil ─► Nullpunkt merken
  ├─ Nur Spule ───────────────────────► MEASURE (Anzeige relativ zum Nullpunkt)
  └─ Box + Spule ─► WAIT_BOX ─ stabil ≥ 50 g UND [Box tarieren] ─► MEASURE (Anzeige relativ zur Box)
MEASURE ─ Spule lag stabil ≥ 50 g auf, danach Waage leer ─► IDLE;  [Fertig] ─► IDLE
```
- Anzeige in MEASURE live: groß das Filament = Spule gesamt − Leerspule (nie < 0), klein das Gesamtgewicht der Spule (mit Box auch das Boxgewicht). Leerspule mit −10/+10 einstellbar (0 bis 2000 g, 0 = ganze Spule), Start `WEIGH_DEFAULT_EMPTY_G` (250 g), der zuletzt eingestellte Wert steht im NVS (`devmode`/`empty`). Nicht stabile Werte erscheinen gedimmt.
- Timeout 120 s in CLEAR, WAIT_BOX und in MEASURE, solange noch keine Spule gewogen wurde. Abbrechen jederzeit. `uiShowHome()` bricht auch diesen Ablauf ab (`simpleFlowAbort`).
- Von Fabian bestätigt (22.11.2026): Leerspulengewicht wird abgezogen (einstellbar); die Webseite verlangt weiterhin eine Anmeldung; der einfache Ablauf steht in der smarten Waage nur Benutzern mit „Nur wiegen“ offen, nicht allen.

---

## 6. Wägezelle

### 6.1 Stabilitätserkennung
- HX711 mit 10 Hz. Gleitendes Fenster über N = 10 Samples (≈ 1 s).
- Stabil, wenn `max − min ≤ 1,0 g` im Fenster (`SCALE_STABLE_G`).
- „Leer“: stabil und `|Gewicht| ≤ 5 g` (`SCALE_EMPTY_G`).
- „Objekt hinzugefügt“: stabil und `Gewicht ≥ Referenz + 50 g` (`WEIGH_ADDED_G`).
- Ausgegeben wird der Mittelwert des stabilen Fensters, intern auf 0,1 g, in der Anzeige auf 1 g gerundet.

### 6.2 Kalibrierung und Tara
- Tara über den Button auf Home (Rechte siehe 11.3). Außerdem automatisches Nullen beim Booten, wenn die Waage leer und stabil ist.
- Kalibrierung: Tara, bekanntes Gewicht auflegen, Wert eingeben. Daraus Faktor berechnen und in NVS speichern.
- Drift: Nullpunkt-Nachführung nur in `IDLE`, nur wenn die Waage leer und stabil ist und die Abweichung kleiner als 2 g ist.

### 6.3 HX711 ohne Daten
- Ohne angeschlossenen HX711 liest die Bibliothek am freien DOUT-Pin lauter Nullen (0) oder Einsen (−1). Mit dem gespeicherten Offset ergibt das eine feste Anzeige wie −67 g. Der `scale`-Task verwirft deshalb die Rohwerte 0, −1 und die Sättigungswerte (`rawInvalid` in `scale/scale.cpp`) und meldet alle 5 s, wie viele es waren (`[SCALE] n ungültige Lesungen verworfen`).
- Treten diese Meldungen mit angeschlossenem HX711 auf, ist die Versorgung oder das SCK-Timing gestört (Kondensatoren 100 µF und 100 nF am HX711 helfen).

### 6.4 Wägesensor wählbar
- Typen (`core/sensor_core.h`): **HX711** (auch HX710B, Standard, GPIO17/18), **NAU7802** (I²C 0x2A, 10 SPS, Gain 128, interner LDO 3,3 V, Offset-Kalibrierung beim Start), **ADS1232/ADS1231** (2-Draht, PDWN optional, Gain/SPEED per Lötbrücke).
- Gemeinsam für alle: Fenster, Stabilität, Tara, Kalibrierung (`scale/scale.cpp`). Je Sensor nur ein Treiber mit `begin/ready/read` (`scale/sensor.h`). Neuer Sensor: Eintrag in `SensorType`, Treiberklasse in `sensor_drivers.cpp`, Fall in `sensorCreate`, Option und Pinrollen in `web_page.h`.
- Auswahl auf der Webseite unter Kalibrierung (nur Admin), `GET/POST /api/sensor`. Gespeichert in NVS `sensor` (`type`, `p0`–`p2`), wirksam nach dem Neustart. Ungültige gespeicherte Werte fallen auf HX711 an GPIO17/18 zurück.
- Pinprüfung (`sensorValidate`): nur freie Header-Pins (2, 4, 6, 7, 8, 9, 10, 14, 16, 17, 18, 21), keine doppelten. IO43/44 (UART0) sind bewusst ausgeschlossen. NAU7802 darf zusätzlich genau SCL = 47 / SDA = 48 nutzen (Touch-Bus, mit dem I²C-Mutex), sonst eigener Bus `Wire1`.
- Kalibrierung pro Typ: HX711 mit den alten Schlüsseln (`offset`, `factor`, `cal`), sonst mit Anhang (`_nau`, `_ads`). Die bestehende HX711-Kalibrierung bleibt beim Update gültig.
- Rohwerte: 0 und −1 gelten bei HX711/ADS1232 als „kein Sensor“, beim NAU7802 nicht (Lesefehler erkennt dort der I²C-Status). Sättigung ist bei allen ungültig. Findet `begin()` den NAU7802 nicht, versucht der Task es alle 2 s erneut, ebenso nach 2 s ohne Daten.
- **Am Gerät nicht geprüft:** NAU7802, ADS1232, die Sensorwahl auf der Webseite und der Umbau des HX711-Pfads auf die Treiber-Schnittstelle.

---

## 7. Spoolman-Integration

Geprüft gegen **Spoolman 0.27.0** (`http://spoolman.local:7912`). API-Präfix `/api/v1`. Bei Änderungen an der Anbindung die API-Doku der laufenden Instanz unter `<basis>/api/v1/docs` prüfen, weil sich Feldnamen zwischen Versionen ändern können.

### 7.1 Client
- `net/spoolman.cpp` läuft in einem eigenen Task (16 KB Stack, Kern 0) mit Auftragswarteschlange, damit blockierende HTTP-Anfragen Weboberfläche und OTA nicht aufhalten.
- URL: `SPOOLMAN_URL` in `secrets.h`, änderbar auf der Webseite (NVS). Erreichbarkeitsprüfung alle 30 s (`/api/v1/info`). Timeout 5 s pro Request (`SPOOLMAN_TIMEOUT_MS`).
- Bei jedem erkannten Spulen-Tag (oder Tag ohne bekanntes Format) wird die Spule nachgeschlagen (`app/session.cpp`). Anzeige: Menü → NFC, Webseite → Spoolman.
- Große Antworten (Spulenliste) mit ArduinoJson-Filter (`DeserializationOption::Filter`) parsen, nur benötigte Felder; Puffer im PSRAM.
- Fehlertexte kommen als `{"message": "..."}` (404 bei unbekannter ID).

### 7.2 Endpunkte
| Zweck | Request |
|---|---|
| Spule per ID | `GET /api/v1/spool/{id}` |
| Spule per Tag-UID | `GET /api/v1/spool?tag=<UID>&allow_archived=true&limit=1` (Antwort `[]` = unbekannt) |
| Tag verknüpfen | `POST /api/v1/spool/{id}/tag` mit `{"uid":"04A1B2C3D4E5F6","format":"ntag"}` (409 = Tag gehört schon zu einer anderen Spule) |
| Gewicht, Box, Owner speichern | `PATCH /api/v1/spool/{id}` mit `remaining_weight`, `location`, `extra` |
| Auswahlliste | `GET /api/v1/spool?limit=60` |
| Katalogabgleich | `GET /api/v1/vendor`, `GET /api/v1/filament` |

- Spoolman normalisiert die UID (Großbuchstaben, ohne Trennzeichen). Das alte Konzept mit Extra-Feld `nfc_uid` und clientseitiger Suche in der Spulenliste entfällt.
- `remaining_weight` ist laut API nur erlaubt, wenn die Filamentsorte ein Füllgewicht (`filament.weight`) hat, sonst kommt HTTP 400 (der Client gibt den Text aus). Alternative: `PUT /api/v1/spool/{id}/measure` mit dem Bruttogewicht (`weight`), das Spoolman über das Leerspulengewicht verrechnet.
- **Achtung Extra-Felder:** Ein `PATCH` mit `extra` ersetzt **alle** Extra-Felder der Spule. Der Client liest deshalb vorher die Spule und führt die Felder zusammen. Werte sind JSON-kodierte Strings, der String selbst steht in Anführungszeichen: `{"extra": {"owner": "\"Fabian\""}}`. Beim Lesen entsprechend dekodieren.

### 7.3 Owner
- Der angemeldete Benutzer wird als Extra-Feld `owner` an die Spule geschrieben. Fehlt das Textfeld in Spoolman, legt der Client es selbst an (`[SPOOLMAN] Extra-Feld 'owner' angelegt`). Ohne angemeldeten Benutzer bleibt das Feld unverändert.

### 7.4 Unbekannte Spule
- `screenAssignSpoolOpen`: Erkennt die Waage einen Spulen-Tag (oder einen Tag ohne bekanntes Format), den Spoolman nicht kennt, öffnet sich auf Home automatisch die Auswahl aus der Spoolman-Spulenliste (max. 60, gefiltert und gestreamt mit HTTP/1.0).
- Danach: Tag verknüpfen (`POST /spool/{id}/tag`), Spule nachschlagen, Tag mit `spoolman_id` beschreiben (vorhandene OpenSpool-Daten bleiben erhalten).
- Auch über Menü → NFC → „Spule zuordnen“ erreichbar, solange ein unbekannter Tag aufliegt.

### 7.5 Offline-Warteschlange
- `core/queue_core` (Logik, nativ getestet), `storage/queue` (Datei). Updates, die wegen Netzwerkfehler oder HTTP 5xx scheitern, landen als JSON-Zeile (`spool_id`, `remaining_weight`, `location`, `timestamp`) in `/queue.jsonl` (LittleFS), je Spule ein Eintrag (neuere Updates werden mit älteren zusammengeführt, `archive`, `owner` und `tag_uid` gehen nicht verloren), max. 100.
- Nachgeholt wird automatisch alle 15 s, sobald Spoolman erreichbar ist. Dauerhafte Fehler (HTTP 4xx) werden verworfen und gezählt. `SpoolmanResult.queued` zeigt, dass ein Update eingereiht wurde.
- Zeitstempel per NTP (`Europe/Berlin`), 0 ohne Uhr.
- Home zeigt „Ausstehende Updates: n“, die Webseite (Spoolman) bietet „Jetzt senden“ und „Alle verwerfen“.
- **`uploadfs` löscht die Warteschlange**, vorher prüfen, ob Updates ausstehen.

### 7.6 Katalogabgleich
- `doCatalogSync` in `net/spoolman.cpp`, `catalogMerge` in `storage/catalog.cpp`. Neue **Hersteller** (`GET /vendor`) und **Materialien** (aus `GET /filament`, Feld `material`) werden in den Katalog **ergänzt**. Vorhandene Einträge bleiben unverändert, gelöscht wird nichts (Namensvergleich ohne Groß-/Kleinschreibung).
- Automatisch alle `CATALOG_SYNC_MS` (10 min), sobald Spoolman erreichbar ist, und auf Knopfdruck (Webseite → Filament-Katalog → „Jetzt abgleichen“, `POST /api/catalog/sync`, Ergebnis `GET /api/catalog/sync`).
- Temperaturbereich eines neuen Typs: kleinster und größter `settings_extruder_temp` der Filamente dieses Materials; bei nur einem Wert ±10 °C (Annahme); ohne Wert übliche Werte (PLA 190–220, PETG 220–250, ABS 230–260, ASA 240–260, TPU 210–230, PA 250–280, PC 260–300, sonst 190–230).
- Nicht übernommen werden zu lange Namen (Typ über 15, Hersteller über 31 Zeichen) oder Einträge bei voller Liste (24). Farben werden nicht abgeglichen.
- Mit 10 KB Stack stürzte der Abgleich beim Speichern ab, deshalb 16 KB; `Catalog` liegt im Merge auf dem Heap.

### 7.7 Debug-Endpunkte (mit Passwort)
- `POST /api/spoolman/lookup?uid=..&id=..`, `/api/spoolman/update?id=..&remaining=..&location=..&owner=..`, `/api/spoolman/link?id=..&uid=..`. Ergebnis über `GET /api/spoolman` (`lookup`, `write`).

### 7.8 Trocknungs-Erinnerung
- Logik in `core/drying_core` (reine Funktionen, Tests `test/test_drying`), Einstellungen in `storage/drying` (NVS-Namespace `drying`, Schlüssel `cfg` als JSON).
- Je Material ein Intervall in Tagen (0 = keine Erinnerung), dazu ein Intervall für alle anderen Materialien (`default_days`, Standard 0) und ein **Box-Faktor** (1 bis 10, Standard 3). Ist an der Spule ein Ort (Box) gesetzt, gilt sie als luftdicht gelagert und das Intervall wird mit dem Faktor multipliziert. Namensvergleich ohne Groß-/Kleinschreibung. Richtwerte beim ersten Start (Annahme, nicht geprüft): PLA 90, PETG 30, ABS 60, ASA 60, TPU 30, PA 14, PC 14, PVA 7 Tage.
- **Zuletzt getrocknet:** Extra-Feld `last_dried` (Text, `JJJJ-MM-TT`, lokales Datum) an der Spule in Spoolman; der Client legt es bei Bedarf an (wie `owner`, andere Extra-Felder bleiben erhalten). Eintragen: `spoolmanRequestMarkDried`. Dieser Auftrag wird bei Netzwerkfehlern **nicht** in die Offline-Warteschlange gestellt, Fehler zeigt `GET /api/spoolman` (`write`). Ohne gestellte Uhr (NTP) ist Eintragen nicht möglich.
- Status: `Due` (Intervall abgelaufen, genau am letzten Tag + Intervall), `Ok` (noch n Tage), `NoRecord` (nie eingetragen, **keine** Warnung) und `Off` (Erinnerung aus, Material ohne Intervall oder Uhr unbekannt).
- Anzeige am Gerät: „Trocknen fällig!“ auf dem Screen „Spule erkannt“ des Wiegeablaufs (nur Hinweis, der Screen steht nur kurz). Menü → NFC (Admin) zeigt Status und Datum der aufliegenden Spule und bietet **Trocknung eintragen** (heutiges Datum).
- Webseite (Admin): Spoolman → Karte „Trocknung“ (Einstellungen), „Zuletzt gescannte Spule“ zeigt den Status und hat den Button „Trocknung eintragen“. API: `GET/POST /api/drying` (Einstellungen), `POST /api/drying/log?id=<Spoolman-ID>`, `/api/spoolman` liefert je Spule `last_dried` und `drying` (`state`, `interval_days`, `days_left`).

---

## 8. Architektur

### 8.1 Tasks (FreeRTOS)
| Task | Aufgabe | Kern |
|---|---|---|
| `ui` | LVGL (`lv_timer_handler`), Screens, Wiegeablauf | 1 |
| `scale` | HX711 lesen, Filter, Stabilität | 1 |
| `nfc` | PN532 pollen (ca. 4 Hz), Tags parsen und schreiben | 1 |
| `net` | WLAN, Webserver, OTA | 0 |
| `spoolman` | Spoolman-Aufträge, Warteschlange, Katalogabgleich | 0 |
| `session` | Benutzer-/Admin-Anmeldung, Spulen-Nachschlagen | 0 |

- **LVGL ist nicht threadsafe.** LVGL-Funktionen ausschließlich im `ui`-Task aufrufen.
- Datenfluss in die UI: Die anderen Tasks senden Events über Queues oder schreiben in mutex-geschützte Strukturen, die die UI per `lv_timer` liest. Aufträge an `spoolman` gehen per Queue, Ergebnisse ebenso zurück.
- **WLAN-Zugriffe nur im `net`-Task.** Scan und Verbindungsaufbau der USB-Einrichtung stellen andere Tasks als Anfrage (`wifiScanRequest`, `wifiProvision` in `net/wifi`, mutex-geschützt), `wifiLoop` führt sie aus. Logik in `core/wifi_core` (`ScanJob`, `ProvisionJob`, nativ getestet, `test/test_wifi`). Der Scan läuft asynchron (`WiFi.scanNetworks(true, …)`), ein Ergebnis gilt 10 s, ein Fehler wird 3 s gemeldet. Während eines Verbindungsversuchs startet kein Scan und die Hotspot-Automatik ruht; ein laufender Scan wird vorher zu Ende gebracht.
- Der I²C-Bus gehört nur dem Touch (`bus/i2c_bus`, Mutex). Der Touch wird direkt über `Wire` gelesen, nicht über den LovyanGFX-Touchtreiber, weil der Bus per Mutex geschützt ist. Das PN532 hängt per SPI an eigenen Pins und kann den Touch nicht blockieren.

### 8.2 Dateistruktur
```
include/
  pins.h            Pinbelegung
  config.h          Defaults (Schwellen, Timeouts)
  lv_conf.h         LVGL 9.5.0
  secrets.h         WLAN, Spoolman-URL, OTA-Passwort (nicht einchecken; Vorlage secrets.h.example)
data/theme.json     UI-Style (LittleFS)
src/
  main.cpp                    Init, Task-Start, net-Task
  app/session                 Anmeldung (Benutzer/Admin), Spulen-Nachschlagen
  bus/i2c_bus                 I²C-Mutex (Touch)
  core/queue_core             Warteschlangen-Logik (ohne Arduino, getestet)
  core/weigh_core             Wiegeablauf-Logik (ohne Arduino, getestet)
  core/simple_core            Einfache Waage, Ablauf-Logik (ohne Arduino, getestet)
  core/drying_core            Trocknungs-Erinnerung: Intervalle, Datum, Status (ohne Arduino, getestet)
  core/update_core            Online-Update: Versionsvergleich, GitHub-Release auswerten (ohne Arduino, getestet)
  core/diag_core              Selbstdiagnose: Zustände der Komponenten → Befund-Codes (ohne Arduino, getestet)
  core/wifi_core              WLAN-Aufträge: Scan, Verbindungsversuch der USB-Einrichtung (ohne Arduino, getestet)
  display/display             LovyanGFX-Setup, LVGL-Flush/Touch, Panel-Tuning
  i18n/i18n                   UI-Texte Deutsch/Englisch
  net/wifi, ota               WLAN, Hotspot, ArduinoOTA
  net/usbsetup                Einrichtung per USB (Webseite, 12.1)
  net/fwupdate                Online-Update aus den GitHub-Releases (eigener Task)
  net/webserver, websession   Weboberfläche, Login, Rollen
  net/web_page.h              HTML/JS der Weboberfläche
  net/spoolman                Spoolman-Client
  nfc/ndef, tags, nfc         siehe 4.6
  core/sensor_core            Wägesensor: Typen, Pinprüfung, Rohwerte (ohne Arduino, getestet)
  core/web_guard              Webschutz: Host-Header und Pflicht-Header für POST (ohne Arduino, getestet)
  scale/scale                 Sensorwahl, Filter, Stabilität, Kalibrierung
  scale/sensor, sensor_drivers  Treiber-Schnittstelle LoadSensor, HX711, NAU7802, ADS1232
  storage/admin, users        Admin, Benutzer, PINs
  storage/catalog             Filament-Katalog
  storage/devicepw            Hotspot- und OTA-Passwort
  core/devicepw_core          Passwort erzeugen, Anzeige-Regel (ohne Arduino, getestet)
  storage/drying              Trocknungs-Einstellungen (NVS), Status je Spule
  storage/devmode             Betriebsart (smart/einfach), Leerspule der einfachen Waage
  storage/queue               Offline-Warteschlange (Datei)
  ui/                         Screens (ui.cpp, screens_menu.cpp, screen_weigh.cpp, screen_simple.cpp), theme, fonts
  util/log, secret            Log-Ringpuffer; Zufall und Salted-SHA-256 für Passwörter/PINs
test/                         Unity-Tests für env:native
tools/screenshots/            Screens am Rechner rendern (siehe 13.3)
```

### 8.3 Screens (240×320, Hochformat)
- Home/IDLE (Live-Gewicht, Statusleiste, Hinweis „Spule scannen“, [Tara] [Menü]), Spule erkannt (Hersteller, Material, Farbfeld aus `color_hex`, letztes Restgewicht), Box auflegen, Spule einstellen (Gewicht relativ zu W1), Bestätigen, Ergebnis, Menü (Meine Sprache bei angemeldetem Benutzer, Einstellungen, NFC, Admin abmelden; unter Einstellungen: Kalibrierung, WLAN, Display, Sprache, Touch-Kalibrierung).
- Buttons mindestens 48 px hoch (Single-Touch, kleines Display). Pro Screen nur eine Hauptaktion.

---

## 9. Konfiguration (NVS)
- WLAN SSID/Passwort (siehe 12.1), Spoolman-Basis-URL
- Kalibrierfaktor, Nulloffset pro Sensortyp (Namensraum `scale`, siehe 6.4)
- Wägesensor: Typ und Pins (Namensraum `sensor`)
- Schwellen (Stabilität, „leer“, „hinzugefügt“, Timeout), Defaults in `config.h`
- Bildschirmschoner, Display-VCOMS, Gerätesprache, Benutzer, Katalog, Admin-Daten
- Betriebsart und Leerspule der einfachen Waage (`devmode`)
- Trocknungs-Erinnerung (`drying`, siehe 7.8)

---

## 10. Coding-Regeln
- UI-Texte und Kommentare **Deutsch**, Bezeichner **Englisch**.
- Module mit klaren Schnittstellen, keine globalen Variablen außerhalb des eigenen Moduls.
- Keine blockierenden `delay()`-Aufrufe im `ui`-Task.
- Größere Puffer (JSON, Listen) im PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`). Den LVGL-Draw-Buffer zuerst im internen DMA-fähigen RAM anlegen (Teilpuffer, z. B. 240 × 40 Pixel), erst bei Bedarf ins PSRAM.
- Logging über `logPrintf`/`logPrintln` (`util/log`, nicht direkt `Serial`) mit Modul-Präfix, z. B. `[SCALE]`, `[NFC]`, `[NET]`.
- Keine Annahmen über unbekannte Pins oder API-Felder: als `TODO` markieren und nachfragen.
- **Tests:** `pio test -e native` (Unity, am Rechner) prüft `nfc/ndef`, `nfc/tags`, `nfc/cbor`, `nfc/openprinttag`, `core/queue_core`, `core/weigh_core`, `core/simple_core`, `core/drying_core`, `core/update_core`, `core/diag_core`, `core/sensor_core`, `core/spool_url`, `core/usb_setup_core` und `core/wifi_core`. Neue Logik ohne Hardwarezugriff in eine Datei ohne Arduino-Abhängigkeit legen, dort testen und in `build_src_filter` des Envs `native` eintragen.
- UI-Texte in `src/i18n/i18n.cpp` (UTF-8): neuen Eintrag in `Str` und in die Tabelle aufnehmen, beide Sprachen. Texte der Webseite in `I18N` in `net/web_page.h`, ebenfalls beide Sprachen.

---

## 11. Benutzer, Admin, Rechte, Sprache

### 11.1 Benutzer (Besitzer)
- Die Webseite legt Benutzer an (`storage/users.cpp`, NVS, max. 16, ID fortlaufend) und löst das Schreiben des Benutzer-Tags aus. Benutzer legen sich nicht selbst an, das geht nur mit Admin-Rechten.
- Ein Benutzer-Tag an der Waage meldet an (`app/session.cpp`), maßgeblich sind ID und Token. Ein gelöschter Benutzer wird abgemeldet und sein Tag ist ungültig.
- **Token pro Benutzer:** Die ID allein meldet nicht an, sonst könnte jeder per Handy-App einen Tag mit `"id":1` schreiben. „Tag schreiben“ erzeugt ein neues geheimes Token (32 Hex, gespeichert mit `usersSetTagToken` sobald der Schreibauftrag angenommen ist, NVS-Schlüssel `tok<id>` im Namespace `users`), das auf den Tag kommt; es gilt nur der zuletzt geschriebene Tag (wie beim Admin-Tag). Prüfung mit `tagCheckUserToken` (`nfc/tags`, nativ getestet, Vergleich ohne frühen Abbruch). Der Inhalt von Benutzer-Tags geht wie beim Admin-Tag nicht an `/api/status`. Ein Tag lässt sich weiterhin 1:1 kopieren; das Token verhindert nur selbst geschriebene Tags.
- **Migration:** Tags aus älteren Versionen haben kein Token und melden nicht mehr an. Das Gerät zeigt „Tag veraltet, im Web neu schreiben“, die Webseite markiert „Tag schreiben“ mit ⚠, solange für den Benutzer noch kein Tag mit Token geschrieben wurde (`"tag":false` in `/api/users`). Am Gerät nicht geprüft.
- Abmeldung nach `USER_TIMEOUT_MS` (15 min) ohne Spulen-/Box-Scan oder per Tippen auf den Namen in der Statusleiste von Home.

### 11.2 Admin, Passwort, Geräte-PIN, Sperre
- **Admin** (`storage/admin.cpp`, NVS): darf alle Einstellungen am Gerät und auf der Webseite ändern, Benutzer anlegen und die Sperre abschalten. Normale Benutzer dürfen nur wiegen (Spule/Box, Spoolman-Update) und ihre eigene Sprache wählen.
- Der Admin kann einzelnen Benutzern das **Tara-Recht** (`User.canTare`, `POST /api/users/tare?id=&allow=1|0`) und das Recht **Filament** (Katalog pflegen, `User.canCatalog`, `POST /api/users/catalog?id=&allow=1|0`) geben, außerdem die Rolle **Nur wiegen** (5.4).
- **Passwort:** am Anfang `Filament`, vom Admin auf der Webseite änderbar (4 bis 32 Zeichen, kein `:`). Gespeichert nur als Salted-SHA-256-Hash. Die Webseite warnt, solange das Standardpasswort aktiv ist.
- **Geräte-PIN:** Die Bildschirmtastatur ist auf dem kleinen Display nicht nutzbar. Am Gerät meldet sich der Admin deshalb mit einer **Geräte-PIN** (4 bis 6 Ziffern, Ziffernblock) oder dem Admin-Tag an. Am Anfang `1234` (`ADMIN_DEFAULT_PIN`), änderbar auf der Webseite (Benutzer → Admin, `POST /api/admin/pin`, Salted-SHA-256 im NVS); die Seite warnt, solange die Standard-PIN aktiv ist. Das Passwort gilt nur für die Webseite. Nach 5 falschen Eingaben ist die Eingabe 30 s gesperrt.
- **Admin-Tag:** Die Webseite erzeugt ihn (Benutzer → Admin → Admin-Tag schreiben); es gilt immer nur der zuletzt erzeugte Token. Ein NFC-Tag lässt sich kopieren, das Passwort ist sicherer.
- **Sperre** (`adminLockEnabled`, am Anfang **an**): Der Admin kann sie auf der Webseite (Benutzer → Admin) abschalten. Ist sie aus, darf jeder am Gerät und im Netzwerk alles ändern.
- **Notfall:** serieller Befehl `admin reset` stellt Passwort `Filament`, Geräte-PIN `1234` und die Sperre wieder her und macht den Admin-Tag ungültig.

### 11.3 Gerät
- Das Menü ist für alle offen. **Einstellungen** und **NFC** verlangen den Admin (`uiAdminGate`), **Tara** den Admin oder einen angemeldeten Benutzer mit Tara-Recht (`sessionUserMayTare`); in der einfachen Waage darf jeder tarieren.
- Die Admin-Anmeldung endet nach `ADMIN_TIMEOUT_MS` (5 min) ohne Bedienung oder über „Admin abmelden“ im Menü bzw. Tippen auf „Admin“ in der Statusleiste. Danach springt die Anzeige zu Home.

### 11.4 Webseite
- Zuerst eine **Login-Maske** (Name + PIN bzw. Passwort). Sitzungen im RAM (`net/websession.cpp`, Cookie `sid`, HttpOnly, SameSite=Strict, `WEB_SESSION_MS` 30 min ohne Anfrage, max. 6). Rollen über `requireRole` in `net/webserver.cpp`:
  - **Admin** (Name `admin`, Admin-Passwort): sieht und darf alles. Für Skripte zusätzlich Basic-Auth (`curl -u admin:... -H 'X-Requested-With: curl'`).
  - **Benutzer** (Name + eigene **PIN**, 4 bis 6 Ziffern): Gewicht, NFC, WLAN-Status (verbunden ja/nein), Tara-Knopf nur mit Tara-Recht, eigene Sprache und PIN. Kein Log, keine Einstellungen, keine Benutzerliste (alle anderen Schnittstellen antworten 403). Die PIN setzt der Admin (`POST /api/users/pin?id=&pin=`, leer = entfernen); ohne PIN nur Anmeldung per NFC am Gerät. Eigene PIN ändern mit alter PIN (`POST /api/me/pin`).
  - Mit dem Recht **Filament** zusätzlich die Seite **Filament-Katalog** (`GET/POST /api/catalog`, `/api/catalog/sync`).
  - **Generic** (Name `generic`, am Anfang ohne PIN, vom Admin abschaltbar oder mit PIN schützbar): nur wiegen und NFC lesen, kein Tara, keine Sprache.
- PINs nur als Salted-SHA-256-Hash im NVS (`pin<ID>`, ID 0 = Generic). Eine geänderte PIN, ein gelöschter Benutzer oder ein geändertes Admin-Passwort beenden die betroffenen Sitzungen. Gilt auch für Admin-Passwort per USB-Einrichtung oder `admin reset` (alle Admin-Sitzungen) sowie für eine geänderte Generic-PIN und das Abschalten von Generic (Generic-Sitzungen). Am Gerät nicht geprüft.
- `/api/status` enthält für Nicht-Admins keine Gerätedaten (IP, Speicher, Firmware) und keinen Inhalt eines Admin-Tags. Ist die Sperre aus, ist jeder Admin.
- Fehlversuch-Sperre **pro Client-IP** (`core/auth_throttle`, Tabelle mit 16 IPs): 5 Fehlversuche (Login, Basic-Auth, alte PIN beim PIN-Wechsel, gemeinsam gezählt) sperren diese IP 30 s, jede weitere Sperre doppelt so lang, höchstens 15 min. Andere IPs bleiben frei, ein Angreifer im LAN sperrt den Admin also nicht mehr aus. Nur eine erfolgreiche Admin-Anmeldung setzt den Zähler der IP zurück (sonst könnte man zwischen Rateversuchen mit eigener PIN oder als Generic den Zähler leeren); nach 1 h ohne Fehlversuch fängt die IP von vorn an. Ist die Tabelle voll, wird der am längsten ruhige ungesperrte Eintrag ersetzt. Grenzen: Wer viele IPs nutzt, bekommt pro IP eigene Versuche; Clients hinter einem gemeinsamen NAT/Proxy teilen sich einen Zähler. Am Gerät nicht geprüft.
- **CSRF und DNS-Rebinding** (`core/web_guard`, `requestAllowed` in `net/webserver.cpp`): Jede Route prüft den Host-Header. Erlaubt sind IP-Adressen, `<OTA_HOSTNAME>` und `<OTA_HOSTNAME>.local`, sonst 403 (im Hotspot Umleitung auf die Einrichtungsseite). Router-Namen wie `filamentwaage.fritz.box` sind damit gesperrt. Alles außer GET braucht den Header `X-Requested-With` (beliebiger Wert), die Weboberfläche schickt ihn mit. Fremde Seiten können ihn nur mit CORS-Preflight setzen, den die Waage nicht beantwortet. Das gilt auch bei ausgeschalteter Sperre und im Hotspot.
- Wer im **geschützten** Setup-WLAN ist (Hotspot-Passwort, 13.2), braucht keine zweite Anmeldung. Ein offener Hotspot zählt nicht als Ausweis.

### 11.5 Sprache pro Benutzer
- `User.lang` (0 = Gerätesprache, 1 = Deutsch, 2 = Englisch, im Benutzer-JSON im NVS). Beim Anmelden per Tag geladen (`i18nApply`, speichert nichts), beim Abmelden kommt die Gerätesprache zurück (`i18nRestoreDefault`).
- Der Benutzer ändert sie am Gerät unter Menü → Meine Sprache, der Admin auch auf der Webseite. Menü → Einstellungen → Sprache und die Webseite stellen die **Gerätesprache** ein (`i18nDefault`).

---

## 12. Weitere Funktionen

### 12.1 WLAN-Einrichtung (wie bei Tasmota)
- Zugangsdaten stehen nur im NVS (Webseite → WLAN oder serielle Konsole). `WIFI_SSID`/`WIFI_PASSWORD` aus `secrets.h` werden **einmalig** beim ersten Start übernommen (Platzhalter `dein-wlan` zählt nicht).
- Ohne gespeicherte Daten startet **sofort** der Hotspot `Filamentwaage-Setup` (Passwort = Hotspot-Passwort, siehe 13.2) und Home zeigt „WLAN einrichten: …“ mit dem Passwort. Ein Captive Portal leitet die Prüf-URLs der Betriebssysteme und jeden fremden Host auf `http://192.168.4.1/#wifi` (Wildcard-DNS, `net/webserver.cpp`, `captiveRedirect`).
- Kann sich das Gerät mit gespeicherten Daten eine Minute lang nicht verbinden, startet der Hotspot ebenfalls, parallel zu den Verbindungsversuchen. Home zeigt dann das Passwort nicht (13.2). Er endet, sobald das Gerät eine Minute stabil verbunden ist (`WIFI_AP_STABLE_MS`) und niemand daran hängt.

- **Einrichtung per USB (Standardweg nach dem Flashen):** Webseite `docs/bauen.html#einrichten` (`docs/setup.js`, Web Serial, 115200 Baud) schickt SSID, WLAN-Passwort und neues Admin-Passwort. Protokoll: Improv-Serial-Rahmen (`IMPROV`, Version 1, Typ, Länge, Daten, 8-Bit-Prüfsumme, `\n`) mit eigenen RPC-Befehlen `0x80` Hello (Antwort: Name, Firmware, Chip, gespeicherte SSID, Standardpasswort aktiv), `0x81` Setup (`[SSID, WLAN-Passwort, Admin-Passwort]`), `0x82` Netzwerkliste (je Netz ein Ergebnis, zum Schluss ein leeres). Logik in `core/usb_setup_core` (nativ getestet), Gerät in `net/usbsetup`, Aufruf aus `loop()` vor den Textbefehlen.
- Ablauf Setup: Prüfung (SSID 1 bis 32, WLAN-Passwort leer oder 8 bis 63, Admin-Passwort wie 11.2 und nicht `Filament`) → Zustand „Provisioning“ → `wifiProvision` lässt den `net`-Task ohne Neustart verbinden (max. 20 s) → nur bei Erfolg WLAN-Daten und Admin-Passwort speichern → Zustand „Provisioned“ + Ergebnis mit `http://<IP>/`. Bei Fehler Fehlerpaket (`0x03` keine Verbindung, `0x80`–`0x82` ungültige Eingabe), nichts wird gespeichert, die alten Zugangsdaten bzw. der Hotspot bleiben. Der Hotspot endet wie üblich nach einer Minute stabiler Verbindung.
- Die Improv-Standardbefehle (`0x01`–`0x04`) beantwortet die Waage absichtlich nicht, sonst fragt der ESP-Web-Tools-Dialog zusätzlich nach dem WLAN. Paketinhalte gehen nie in den Zeilenpuffer oder das Log, Passwörter werden nach der Verarbeitung im RAM überschrieben. Wer USB-Zugang hat, kann ohnehin `admin reset` ausführen, deshalb ist die Einrichtung jederzeit möglich, nicht nur beim ersten Start.
- **Am Gerät nicht geprüft:** ganzer USB-Einrichtungsablauf, insbesondere ob sich der native USB-Port nach dem Flashen neu anmeldet (dann fragt der Browser erneut nach dem Anschluss) und ob das Öffnen des Ports die Waage neu startet (die Seite fragt bis zu 16 s nach).

### 12.2 Serielle Notbedienung
- USB, 115200 Baud, Zeilenende `\n`: `wifi set <SSID>|<Passwort>`, `wifi reset`, `wifi status`, `admin reset` (siehe 11.2), `reboot`.

### 12.3 Log
- `util/log` schreibt alle Ausgaben und die ESP-IDF-Meldungen in einen Ringpuffer (160 Zeilen, PSRAM). Webseite → Log (`GET /api/log?since=N`, Header `X-Log-Seq`). Arduino-eigene `log_e`-Meldungen des Cores erscheinen dort nicht.

### 12.4 Bildschirmschoner
- Nach `dim` Minuten ohne Berührung, Tag-Scan, Gewichtsänderung (> 3 g) oder Meldung auf 10 % gedimmt, nach `off` Minuten aus (Standard 5/15, 0 = nie, NVS, Webseite → System). Der erste Tipp weckt nur und löst nichts aus.

### 12.5 Weboberfläche
- Menü: Übersicht, Kalibrierung, Benutzer, Filament-Katalog, Spoolman, WLAN, Log, System. `GET /api/wifi/scan` blockiert nicht: 202 = Suche läuft (die Seite fragt alle 0,8 s nach, höchstens 20-mal), 200 = Liste, 503 = Suche gescheitert. Zweisprachig, folgt der Gerätesprache. Login siehe 11.4. Die Seite `/` geht mit `Cache-Control: no-cache` raus, damit der Browser nach einem Firmware-Update nicht die alte Seite aus dem Cache zeigt (bei v0.2 blieb die Seite deshalb schwarz, bis Strg+Shift+R). Am Gerät nicht geprüft.
- Design wie die Projektseite (`docs/style.css`): dunkel, enge große Headlines, Pill-Buttons und -Navigation, weicher Farbverlauf, Karten, Akzent blau. Tokens als CSS-Variablen in `web_page.h` (`:root`), systemeigene Schriften (offline, kein Webfont/CDN). Am Gerät nicht geprüft.
- **Ausliefern der Seite:** `tools/web_page_gz.py` (Pre-Skript) packt `net/web_page.h` beim Bauen mit gzip nach `.pio/build/<env>/gen/web_page_gz.h` (~26 statt ~90 KB). `GET /` schickt die gepackte Seite, wenn der Browser gzip annimmt, sonst die ungepackte, immer mit `Cache-Control: no-cache` und `ETag` (Hash der Seite); bei passendem `If-None-Match` kommt 304 ohne Inhalt. `WaageWebServer` (`net/webserver.cpp`) verwirft eine leere Verbindung nach 200 ms, wenn eine weitere wartet: Der Arduino-`WebServer` bedient nur eine Verbindung gleichzeitig und wartet sonst bis zu 5 s auf eine vorsorglich geöffnete Browser-Verbindung (Preconnect), während die echte Anfrage wartet. Kein Favicon-Abruf (`<link rel="icon" href="data:,">`). **Am Gerät nicht geprüft.**

### 12.6 Online-Update (GitHub-Releases)
- Webseite → System → Firmware-Update (nur Admin): „Nach Updates suchen“ fragt `https://api.github.com/repos/<FW_UPDATE_REPO>/releases?per_page=15` (ohne Token, Limit 60 Anfragen/h je IP; Entwürfe zählen nicht) und wählt die höchste Version, die der Update-Kanal erlaubt. Die Antwort wird direkt aus dem HTTP-Stream mit ArduinoJson-Filter gelesen (nie als ganzer String im RAM); nur die gefilterten Felder bleiben im Speicher. Am Gerät nicht geprüft. Ist sie neuer als die eingebaute Version, zeigt die Seite Version und Release Notes (auf 3000 Byte gekürzt) und „Jetzt aktualisieren“.
- Update-Kanäle (NVS `fwupd`/`ch`, Standard Stable), jeder enthält die stabileren: **Stable** = Tag ohne Bindestrich (`v1.2`), auf GitHub keine Pre-Release. **Release-Kandidat** = zusätzlich Tags mit `-rc` (`v1.3-rc1`) und Pre-Releases ohne Bindestrich. **Test** = zusätzlich jeder andere Bindestrich-Zusatz (`v1.3-beta1`, `v1.3-test2`). Reihenfolge bei gleicher Nummer: Test < Release-Kandidat < Stable, darin nach der Zahl am Ende (`beta2` < `beta10`).
- Liefert der Kanal nur eine ältere Version als die installierte (z. B. Wechsel von Test zurück auf Stable), bietet die Seite „<Version> installieren (Rückschritt)“ an. Ein Rückschritt behält NVS und LittleFS; ob ältere Firmware mit neueren gespeicherten Daten klarkommt, ist nicht geprüft. Übersicht → Gerät zeigt die Version, der Menüpunkt System einen Punkt, wenn ein Update bereitsteht.
- Version: `tools/version.py` (PlatformIO-Pre-Skript) setzt `FW_VERSION` aus der Umgebungsvariable `FW_VERSION` (Release-Workflow: der Tag), sonst `git describe --tags --always --dirty`, sonst `dev`. Vergleich in `core/update_core`: numerisch (`v1.10` > `v1.9`), `v1.2-beta` < `v1.2`, `v1.2-3-gabc` (Stand nach dem Tag) > `v1.2`. Bei unbekannter eigener Version (`dev`, reiner Hash) wird jede stabile Release angeboten.
- Installation in `net/fwupdate.cpp` (eigener Task, 12 KB Stack, Kern 0): Asset `*-ota.bin` per HTTPS laden, Weiterleitungen selbst verfolgen (nur `https://github.com` und `*.githubusercontent.com`), Zertifikate über das eingebaute CA-Bundle des Arduino-Cores (`useBuiltinCACertBundle()`). Vorher Prüfung: zweiter App-Slot vorhanden, Image passt hinein, Größe = Asset-Größe der Release. Schreiben mit `Update`, dabei SHA-256 mitrechnen (mbedTLS); der Vergleich mit dem `digest`-Feld des Assets (`sha256:<hex>`, von GitHub) läuft vor `Update.end()`. Fehlt der Digest (ältere Releases) oder weicht die Prüfsumme ab, wird nicht installiert. `Update.end()` prüft das Image und schaltet erst dann den Boot-Slot um. Bei jedem Fehler `Update.abort()`, die laufende Firmware bleibt aktiv. Nach Erfolg Neustart nach 2,5 s.
- Automatische Prüfung 60 s nach der WLAN-Verbindung, danach täglich (nach Fehler stündlich). Es wird nie automatisch installiert. Im Einrichtungs-Hotspot keine Prüfung.
- Nur die Firmware wird ersetzt. LittleFS (Theme, Katalog, Warteschlange) und NVS (WLAN, Benutzer, Kalibrierung) bleiben. Neue Dateien unter `data/` kommen so nicht aufs Gerät.
- Kein automatischer Rollback: Startet die neue Firmware, aber funktioniert nicht, hilft ArduinoOTA, USB oder der Web-Flasher.
- API: `GET /api/update[?notes=1]`, `POST /api/update/check`, `POST /api/update/install`, `POST /api/update/channel?ch=stable|rc|test` (alle Admin).
- Am Gerät nicht geprüft (Stand der Einführung).

### 12.7 Selbstdiagnose
- Webseite → System → Selbstdiagnose (nur Admin, aktualisiert sich alle 3 s): je Komponente (Wägesensor, NFC-Modul, WLAN, Spoolman, Offline-Warteschlange) eine Zeile mit Status und, bei einem Fehler, Ursache und Lösungstipp. Im Modus „Einfache Waage“ nur Wägesensor und WLAN.
- `core/diag_core` (`diagRun`) macht aus einer Momentaufnahme (`DiagInput`) Befunde (Komponente, Stufe ok/info/warn/error, Code). Die Klartexte stehen zweisprachig in `net/web_page.h` (`dg_<komponente>_<code>`, Tipp `…_tip`). Neuer Befund = Code in `diag_core.cpp`, Test in `test/test_diag`, Texte in beiden Sprachen.
- Sensor-unabhängig: Es zählt nur, ob der Wägesensor gültige Werte liefert (`ScaleReading.valid`), nicht welcher Wandler. Der Tipp „keine gültigen Werte“ gibt es je Sensortyp (`dg_sensor_no_data_tip_<hx711|nau7802|ads1232>`, Typ aus `scaleSensorActive()`, `GET /api/diag` → `sensor`), sonst den allgemeinen.
- Spoolman: `SpoolmanStatus.httpCode` (HTTP-Status der letzten `/api/v1/info`-Prüfung) trennt „nicht erreichbar“ (0), falsche Adresse (404), Zugriff verweigert (401/403), Serverfehler (5xx) und „kein Spoolman“ (200 ohne gültiges JSON).
- In den ersten 30 s nach dem Start gelten fehlende Daten als „startet noch“ (Info), danach als Fehler.
- API: `GET /api/diag` (Admin) liefert `{"items":[{"comp","level","code"}],"http":n}`.
- Am Gerät nicht geprüft. Eine Anzeige am Gerätedisplay gibt es noch nicht.

---

## 13. Flashen, Releases, UI-Style

### 13.1 Flashen
- **Standard: Web-Flasher** auf GitHub Pages (`docs/bauen.html`, ESP Web Tools). Er flasht das Factory-Image der gewählten Release per USB im Browser, ältere stabile Versionen per Auswahlliste.
- Alternativ mit PlatformIO: erstes Flashen per USB `pio run -e waage -t upload`, danach per WLAN (ArduinoOTA, Hostname `filamentwaage.local`): `OTA_PASSWORD=<pw> pio run -e waage-ota -t upload`. Bricht der OTA-Build wegen eines beschädigten Build-Verzeichnisses ab, hilft `rm -rf .pio/build/waage-ota`.

### 13.2 Geräte-Passwörter (Hotspot und OTA)
- `storage/devicepw.{h,cpp}`, Logik in `core/devicepw_core` (nativ getestet, `test/test_devicepw`). Zwei **getrennte** Passwörter, beide 10 Zeichen ohne verwechselbare Zeichen:
  - **Hotspot** (`hotspotPassword()`, NVS `devpw`/`pw`, beim ersten Start zufällig erzeugt): schützt das Einrichtungs-WLAN. Wer im Hotspot ist, gilt in der Weboberfläche als Admin (11.4). Home zeigt es **nur ohne gespeichertes WLAN** (Ersteinrichtung, `devicepw::showOnHome`). Läuft der Hotspot als Rückfall nach WLAN-Ausfall oder per Webseite gestartet, steht auf Home nur „Passwort: Einstellungen > WiFi“. Sichtbar ist es dann nur für den Admin: am Gerät unter Menü → Einstellungen → WiFi (`uiAdminGate`) und im Web unter WLAN → Geräte-Passwörter.
  - **OTA** (`otaPassword()`): schützt ArduinoOTA. Steht in `secrets.h` ein eigenes `OTA_PASSWORD` (mindestens 8 Zeichen, nicht `bitte-aendern`), gilt dieses (`otaPasswordFixed()`). Sonst eigenes Zufallspasswort (NVS `devpw`/`ota`). Erscheint **nie** auf dem Display, nur für den Admin im Web unter WLAN → Geräte-Passwörter. Für `waage-ota` als `OTA_PASSWORD` übergeben.
- Neu erzeugen im Web (Admin): `POST /api/devicepw/regenerate?which=ap|ota` (409, wenn das OTA-Passwort aus `secrets.h` kommt). Ein laufender Hotspot übernimmt das neue Passwort nach 0,8 s per `wifiApApplyPassword()` (`WiFi.softAP` auf dem laufenden AP, ohne Moduswechsel), ArduinoOTA per `otaPasswordChanged()`. `GET /api/wifi` liefert `ap_password`, `ota_password`, `ota_fixed` (Admin).
- Die UI liest die Passwörter im `ui`-Task, geschrieben wird im `net`-Task: feste Puffer unter Spinlock.
- Update von älterer Firmware: Das Hotspot-Passwort bleibt (gleicher Schlüssel `pw`, war bisher auch das OTA-Passwort). Das OTA-Passwort ist danach neu (Webseite). Wer das alte Passwort kannte, sollte das Hotspot-Passwort neu erzeugen.
- Grund: Web-Flasher-Images werden ohne eigene `secrets.h` gebaut und dürfen kein öffentlich bekanntes Passwort enthalten. Früher war ein einziges Geräte-Passwort Hotspot- und OTA-Passwort und stand bei jedem Hotspot-Betrieb auf Home. Wer am Gerät vorbeikam, konnte nach einem WLAN-Ausfall per ArduinoOTA eigene Firmware aufspielen und war im Hotspot Admin.
- Am Gerät nicht geprüft: Anzeige auf Home und unter Einstellungen → WiFi, Karte Geräte-Passwörter im Browser (nur `node --check`), Neu erzeugen bei laufendem Hotspot (ob verbundene Geräte getrennt werden), ArduinoOTA mit dem neuen OTA-Passwort, Übernahme nach dem Neu erzeugen ohne Neustart.

### 13.3 Releases und Web-Flasher
- `.github/workflows/release.yml` läuft bei jedem Tag `v*`. Er baut mit der Vorlage `secrets.h.example` `firmware.factory.bin` (Bootloader 0x0, Partitionen 0x8000, boot_app0 0xe000, Firmware 0x10000, von PlatformIO zusammengesetzt) und legt eine GitHub-Release mit Factory-Image, OTA-Image (`filament-scale-<tag>-ota.bin`) und `docs/cad/filament-scale.step` an.
- Tags mit Bindestrich werden Pre-Releases. `tools/update_flasher.py` schreibt jeden Tag nach `docs/firmware/<tag>/` und `versions.json` (mit Kanal `stable`/`rc`/`test`, gleiche Regeln wie das Online-Update in 12.6; `--prerelease` für Pre-Releases ohne Bindestrich = `rc`). Die höchste **stabile** Version liegt zusätzlich als `docs/firmware/manifest.json` (Standard des Flash-Buttons), Pre-Releases ändern ihn nie. Die Seite `bauen.html` zeigt eine Kanalwahl (nur Kanäle mit Einträgen) und dann die Versionsliste, Vorabversionen mit Warnhinweis. Der Workflow committet das direkt auf `main`. Nachträglich ablegen (z. B. v0.1): Workflow `flasher-sync.yml` per Hand mit dem Tag starten. Web-Flasher-Kanalwahl am Gerät/Browser nicht geprüft.

### 13.4 UI-Style, Schriften, Screenshots
- UI-Style (Farben, Radius, Abstände, Schriftgrößen, Buttonhöhe): `data/theme.json`, aufspielen mit `OTA_PASSWORD=<OTA-Passwort> pio run -e waage-ota -t uploadfs`. Das ersetzt nur das LittleFS-Image (**und damit die Warteschlange**, siehe 7.5). Fehlt die Datei oder ist sie ungültig, gelten die Defaults in `ui/theme.cpp`.
- Schriftgrößen: 14, 20, 28, 48 (`src/ui/fonts/lv_font_app_<größe>.c`, Montserrat Medium). 14/20/28 enthalten Latin-1 (Umlaute, ß) und die LVGL-Symbole, 48 nur ASCII (Gewicht). Die eingebauten Montserrat-Größen 20/28/48 sind in `lv_conf.h` abgeschaltet.
- Neue Schriftgröße oder weitere Zeichen: mit `lv_font_conv` (npm) neu erzeugen, Quellen in `.pio/libdeps/waage/lvgl/scripts/built_in_font/`. Beispiel für 20 px:
  `lv_font_conv --no-compress --no-prefilter --bpp 4 --size 20 --font Montserrat-Medium.ttf -r 0x20-0x7F,0xA0-0xFF,0x2022 --font FontAwesome5-Solid+Brands+Regular.woff -r <Symbolcodes aus built_in_font_gen.py> --format lvgl --force-fast-kern-format --lv-font-name lv_font_app_20 -o src/ui/fonts/lv_font_app_20.c`
  Danach in `ui/fonts/fonts.h` deklarieren und in `ui/theme.cpp` (`fontFor`) eintragen.
- Screenshots: `tools/screenshots/build.sh` rendert die echten Screens am Rechner (macOS und Linux). Referenzen in `screenshots/1x` (Deutsch) und `screenshots/en/1x` (Englisch). Ablauf siehe `.claude/agents/ui-preview.md`. Neue Hardware-/Netzfunktionen, die die UI aufruft, brauchen einen Ersatz in `tools/screenshots/stubs.cpp` (Zustand in `mock.h`), sonst linkt das Werkzeug nicht; neue Quelldateien ohne Arduino-Abhängigkeit kommen in `build.sh`. Die CI (Job `screenshots`) rendert Deutsch und Englisch und vergleicht pixelgenau mit den Referenzen (`tools/screenshots/check.py`). Ändert sich ein Screen absichtlich, Referenzen mit `OUT_DIR=../../screenshots sh build.sh` bzw. `SHOT_LANG=en OUT_DIR=../../screenshots/en sh build.sh` neu erzeugen und mit einchecken (`4x/` nicht).

### 13.5 Display-Feineinstellung
- `display.cpp` überschreibt nach dem Init einige ST7789-Register (`lcdTunePanel`). Mit den LovyanGFX-Defaults (VCOMS 0x28) zeigte das Panel Geisterbilder auf dunklen Flächen, mit dem Demo-Wert 0x19 waren sie dunkler statt heller. Per Sichtprüfung am Gerät (Menü → Einstellungen → Display) wurde `0x24` (36) gewählt, er ist der Standard `DISPLAY_VCOMS_DEFAULT`. Ein gespeicherter NVS-Wert hat Vorrang.
- LCM-Control muss `0x0C` bleiben, `0x2C` aus dem Demo vertauscht Rot und Blau. Diese Werte nicht ohne Sichtprüfung ändern.

---

## 14. Prüfstand

### 14.1 Am Gerät geprüft
- **Spoolman** (Testlauf mit Testdaten, danach gelöscht, 22 von 22 Prüfungen): Lookup per ID und per UID (Hersteller, Material, Restgewicht, Leerspulengewicht vom Hersteller, Füllgewicht, Ort, Owner), Tag verknüpfen inkl. 409-Konflikt, Update von Gewicht/Ort/Owner (Feld `owner` wird angelegt, andere Extra-Felder bleiben erhalten), Umlaute im Owner, Fehlerfall Sorte ohne Füllgewicht (HTTP 400).
- **Katalogabgleich** mit leerer und gefüllter Spoolman-Instanz.
- **Rechte per HTTP** (curl gegen das Gerät): Login der drei Rollen, Rechte-Matrix (Benutzer/Generic bekommen für Log, Benutzerliste, WLAN, Spoolman, Theme, Admin, Neustart 403), Tara-Recht, eigene Sprache und PIN, Beenden der Sitzungen, Sperre aus/an, Fehlversuch-Sperre.
- **Hotspot** über die Webseite ein/aus, Captive-DNS und Weiterleitung.
- **Display-VCOMS** per Sichtprüfung.
- **HX711:** verdrahtet und kalibriert (laut Fabian, 22.11.2026, mit einem früheren Firmware-Stand). Keine ungültigen Lesungen mehr.

### 14.2 Am Gerät nicht geprüft
- **Einrichtung per USB** (12.1): Protokoll nativ getestet, `docs/setup.js` nur mit `node --check` geprüft. Nicht geprüft: Ablauf mit echter Waage und Chrome/Edge, Neuanmeldung des USB-Ports nach dem Flashen, Neustart beim Öffnen des Ports, Netzwerksuche während eines laufenden Verbindungsversuchs.
- **Wiegeablauf** mit echter Waage, Box-Tag und Spoolman, inklusive „Neue Box anlegen“. Die Firmware baut, die Logik ist nativ getestet, die Screens sind aus dem echten UI-Code gerendert.
- Geräte-Passwörter (`devicepw`, 13.2).
- **CSRF/DNS-Rebinding-Schutz** (11.4): Host-Prüfung nativ getestet, Weboberfläche nur mit `node --check`. Nicht geprüft: Zugriff über IP, `.local` und Hotspot im Browser, Captive-Portal, Ablehnung fremder Hosts am Gerät.
- **QR-Code zur Spule** (5.1): Anzeige und Scannen mit einem Handy, Speicherbedarf von `lv_qrcode` auf dem ESP32. Der Screen ist gerendert, der QR-Code in der Vorschau mit OpenCV gelesen.
- **Einfache Waage** und Rolle „Nur wiegen“ (5.4): Ablauf am Gerät, Umschalten auf der Webseite, Benutzer-Spalte. Logik nativ getestet, Screens gerendert, Webseite nur mit `node --check` geprüft.
- **Trocknungs-Erinnerung** (7.8): Logik nativ getestet, Webseite nur mit `node --check` geprüft. Nicht gegen eine echte Spoolman-Instanz geprüft: Anlegen und Schreiben des Extra-Felds `last_dried`, Anzeige am Gerät und Webseiten-Karte.
- Hotspot-Start beim Boot ohne gespeicherte Daten, Beitreten mit einem Handy, automatischer Rückfall nach einer Minute ohne Verbindung.
- Aussehen von Login-Maske und Benutzer-Ansicht im Browser (nur JavaScript-Syntax mit `node --check`), Touch-Anmeldung mit dem Ziffernblock (Layout und Tastenhöhe), Auflegen eines Admin-Tags, Sprachwechsel beim Anmelden am Gerät, Ablauf der Admin-Anmeldung.

- **WLAN-Aufträge im `net`-Task** (8.1): asynchroner Scan (Webseite und USB), Verbindungsaufbau der USB-Einrichtung im `net`-Task. Logik nativ getestet, Webseite nur mit `node --check`.
- **OpenPrintTag** auf NTAG215/216 (4.7): Lesen, Schreiben, Anzeige unter Menü → NFC und auf der Webseite.
- **Selbstdiagnose** (12.7): Seite im Browser, Befunde mit echten Fehlern (HX711/PN532 abgesteckt, Spoolman aus, falsche URL, WLAN weg). Logik nativ getestet, JavaScript nur mit `node --check`.
- **Wägesensor wählbar** (6.4): NAU7802 und ADS1232 ohne Hardware umgesetzt, Webseite nur mit `node --check` geprüft, HX711 nach dem Umbau auf die Treiber-Schnittstelle noch nicht am Gerät.

### 14.3 Offen
- Leseweite des PN532 durch Wiegeplatte und Boxboden praktisch testen.

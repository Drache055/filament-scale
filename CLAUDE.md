# CLAUDE.md – Filament Scale

ESP32-S3-Filamentwaage mit NFC und Spoolman. Fachliche Spezifikation, Hardware und Entwicklernotizen: [`firmware/CLAUDE.md`](firmware/CLAUDE.md). Webseite mit Web-Flasher: `docs/` (GitHub Pages).

## Sprache
- Chat mit Fabian: Deutsch.
- Alles auf GitHub auf Englisch: PR-Titel und -Beschreibungen, Issues, Kommentare, Commit-Messages, Release Notes.
- Im Code: UI-Texte und Kommentare Deutsch, Bezeichner Englisch (siehe `firmware/CLAUDE.md`).

## Arbeitsweise
- Alles läuft über GitHub: Branch, Pull Request, CI. Kein lokaler Build, kein USB am Rechner vorausgesetzt.
- CI (`.github/workflows/ci.yml`) bei jedem Push und PR: `native-tests` (`pio test -e native`) und `firmware-build` (`pio run -e waage`). Eine Firmware-Änderung ist erst fertig, wenn beide Jobs grün sind.
- In der Cloud-Sitzung ist die PlatformIO-Registry gesperrt, `pio` kann dort nichts installieren. Build-Prüfung deshalb über die CI. GitHub-Klone (z. B. LVGL, ArduinoJson, Unity) funktionieren für Tests und Screenshots am Rechner.
- Was nicht am echten Gerät getestet wurde, im PR und in `firmware/CLAUDE.md` als „am Gerät nicht geprüft“ kennzeichnen.

## Releases
- Version per Tag `vX.Y` (z. B. `v1.2`) auf `main`. `.github/workflows/release.yml` baut Factory- und OTA-Image und legt die GitHub-Release mit STEP-Datei an.
- Tag mit Bindestrich (z. B. `v1.2-beta`) = Pre-Release. Pre-Releases kommen nicht in den Web-Flasher.
- Stabile Tags schreibt der Workflow über `tools/update_flasher.py` nach `docs/firmware/` (die höchste Version ist Standard im Web-Flasher). Details: `firmware/CLAUDE.md`, Abschnitt „Flashen und Releases“.
- Tags nur setzen, wenn Fabian es verlangt.

## Subagents (`.claude/agents/`)
- `worker` (Sonnet): klar umrissene Routinearbeit wie Boilerplate, mechanisches Refactoring, Doku, einfache Tests, Suche.
- `expert` (Opus): Architektur, schwer eingrenzbare oder hardwarenahe Fehler, Review nicht-trivialer Änderungen, NFC/PN532, Spoolman-API, HX711.
- `build-checker` (Sonnet): bei rotem Build, Test oder CI. Liefert die Fehlerursache, ändert keinen Code.
- `ui-preview` (Sonnet): nach Änderungen an Screens, Theme oder UI-Texten. Rendert die Screens und vergleicht mit `firmware/screenshots/`, ändert keinen Code.
- Kleine Änderungen (wenige Zeilen, Ort bekannt) direkt erledigen. Planung, Rückfragen, Commits und PRs bleiben beim Hauptagenten.

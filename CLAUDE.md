# CLAUDE.md – Filament Scale

Die Firmware-Spezifikation und Entwicklernotizen stehen in [`firmware/CLAUDE.md`](firmware/CLAUDE.md). Die Webseite liegt in `docs/`.

## Delegationsstrategie (Subagents)

Subagents liegen in `.claude/agents/`.

- **Standard: `worker`** (Sonnet). Delegiere klar umrissene Routineaufgaben proaktiv: Boilerplate, mechanisches Refactoring, Formatierung, Doku, einfache Tests, Datei- und Codesuche.
- **Nur bei Bedarf an Tiefe: `expert`** (Opus). Architektur und Firmware-Design, schwer eingrenzbare Fehler, Code-Review nicht-trivialer Änderungen, Schnittstellen (NFC/PN532, Spoolman-API, HX711-Wägezelle).
- Der Hauptagent behält Planung, Rückfragen an den Nutzer, Commits und PRs.
- Meldet der `worker` eine Designfrage oder unklare Ursache zurück, übernimmt der `expert`. Empfiehlt der `expert` Routinearbeit, geht sie an den `worker`.
- Kleine Änderungen (wenige Zeilen, Ort bekannt) direkt erledigen, ohne zu delegieren.

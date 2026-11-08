#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Offline-Warteschlange für Spoolman-Updates (CLAUDE.md 7.3). Reine Logik ohne Hardwarezugriff.
// Eine Zeile je Update (JSON): {"spool_id":3,"remaining_weight":612.5,"location":"Box 03","owner":"Fabian","timestamp":1760000000}
struct QueueEntry {
  int spoolId = -1;
  float remaining = -1;       // < 0 = nicht ändern
  bool hasLocation = false;
  char location[65] = "";
  char owner[24] = "";        // leer = nicht ändern
  uint32_t timestamp = 0;     // Unix-Zeit, 0 = Uhr war noch nicht gestellt
};

std::string queueEntryToLine(const QueueEntry &e);
bool queueEntryFromLine(const std::string &line, QueueEntry &out);
std::vector<QueueEntry> queueParse(const std::string &text);   // ungültige Zeilen werden übersprungen
std::string queueSerialize(const std::vector<QueueEntry> &list);

// Neuen Eintrag anhängen. Ältere Einträge zur selben Spule werden durch den neuesten ersetzt.
// Bei mehr als maxEntries Einträgen fallen die ältesten weg.
void queueAddEntry(std::vector<QueueEntry> &list, const QueueEntry &e, size_t maxEntries);

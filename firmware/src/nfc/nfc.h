#pragma once
#include <cstddef>
#include <cstdint>

#include "tags.h"

// PN532-Anbindung: Polling (ca. 4 Hz), UID, Capability Container, NDEF lesen/schreiben (CLAUDE.md Abschnitt 4).
struct NfcState {
  bool moduleReady = false;   // PN532 antwortet
  bool tagPresent = false;
  char uid[21] = "";          // Hex, Großbuchstaben, ohne Trennzeichen
  size_t capacity = 0;        // NDEF-Bereich in Byte (aus CC), 0 = kein gültiger CC
  bool locked = false;        // Tag ist (teilweise) schreibgeschützt
  bool hasNdef = false;       // gültiger NDEF-MIME-Record gelesen
  char json[256] = "";        // Payload (gekürzt)
  TagContent content;         // geparster Inhalt, kind = Unknown bei leer/unbekannt
  uint32_t seq = 0;           // zählt hoch, sobald ein Tag neu erkannt wurde
  uint32_t readSeq = 0;       // seq, für die der Inhalt (content) bereits gelesen ist
};

enum class NfcWrite : uint8_t {
  Idle,
  WaitingForTag,
  Writing,
  Ok,
  ErrLocked,
  ErrTooBig,
  ErrNotNtag,  // kein gültiger Capability Container
  ErrFailed,
  ErrTimeout,
  ErrOccupied,  // Web-Auftrag: Tag enthält eine Spule und wird nicht überschrieben
};

// Woher der Schreibauftrag kommt. Web-Aufträge schreiben nur auf einen neu aufgelegten Tag
// und überschreiben keine Spulen-Tags.
enum class NfcWriteOrigin : uint8_t { Device, Web };

void nfcBegin();
NfcState nfcGet();  // threadsicherer Schnappschuss

// Beschreibt den nächsten (bzw. bereits aufliegenden) Tag mit einem JSON-MIME-Record.
bool nfcRequestWrite(const char *json, NfcWriteOrigin origin = NfcWriteOrigin::Device);  // false, wenn schon ein Auftrag läuft
NfcWriteOrigin nfcWriteOrigin();
void nfcAcknowledgeWrite();  // Ergebnis (Ok/Fehler) quittieren, Status wird wieder Idle
NfcWrite nfcWriteStatus();
void nfcCancelWrite();


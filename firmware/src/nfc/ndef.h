#pragma once
#include <cstddef>
#include <cstdint>

// NDEF-Handling für NFC Forum Type 2 (NTAG215/216), ein einzelner MIME-Record (CLAUDE.md Abschnitt 4.1).
// Reine Funktionen ohne Hardwarezugriff.

// Capability Container (Page 3): E1 <Version> <Größe/8> <Zugriff>
struct NdefCc {
  size_t capacityBytes = 0;  // NDEF-Bereich ab Page 4
  bool readOnly = false;     // Schreibzugriff laut CC gesperrt
};
bool ndefParseCc(const uint8_t cc[4], NdefCc &out);

enum class NdefLocate : uint8_t {
  Found,     // NDEF-TLV vollständig im Puffer: recordOffset/recordLength gültig
  NeedMore,  // Puffer zu kurz: neededBytes Bytes ab Page 4 lesen
  None,      // kein (oder leeres) NDEF
  Invalid,
};
struct NdefLocation {
  size_t recordOffset = 0;
  size_t recordLength = 0;
  size_t neededBytes = 0;
};
// data = Rohbytes ab Page 4. Überspringt Null-, Lock-Control- und Memory-Control-TLVs.
NdefLocate ndefLocate(const uint8_t *data, size_t avail, NdefLocation &loc);

struct NdefMime {
  const uint8_t *payload = nullptr;
  size_t payloadLength = 0;
  char type[48] = {0};
};
// Parst den ersten Record. Nur TNF 0x02 (MIME), nicht gechunkt.
bool ndefParseMimeRecord(const uint8_t *rec, size_t len, NdefMime &out);

// Sucht in einer NDEF-Nachricht (mehrere Records möglich, z. B. URI + MIME) den ersten MIME-Record mit dem Typ
// mimeType. Gechunkte Records werden übersprungen.
bool ndefFindMimeRecord(const uint8_t *msg, size_t len, const char *mimeType, NdefMime &out);

// Baut TLV (0x03, Länge, Record) + Terminator 0xFE, aufgefüllt auf ganze Pages (4 Byte).
// Rückgabe: Bytes in out, oder 0, wenn es nicht in capacityBytes bzw. outCap passt.
size_t ndefBuildMimeTlv(const char *mimeType, const uint8_t *payload, size_t payloadLen, size_t capacityBytes,
                        uint8_t *out, size_t outCap);

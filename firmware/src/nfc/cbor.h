#pragma once
#include <cstddef>
#include <cstdint>

// Minimaler CBOR-Leser und -Schreiber (RFC 8949) für OpenPrintTag. Reine Funktionen ohne Hardwarezugriff.
// Unterstützt alle Haupttypen beim Überspringen, auch Längen ohne Angabe (indefinite length).

enum class CborType : uint8_t { UInt, NInt, Bytes, Text, Array, Map, Tag, Simple, Float, Break, Invalid };

struct CborItem {
  CborType type = CborType::Invalid;
  uint64_t value = 0;        // UInt/NInt (NInt: Wert = -1 - value), Länge bei Bytes/Text/Array/Map, Tag-Nummer, Simple
  bool indefinite = false;   // Bytes/Text/Array/Map ohne Längenangabe
  double number = 0;         // Float
  const uint8_t *data = nullptr;  // Bytes/Text mit fester Länge: Inhalt
  size_t headerEnd = 0;           // Position direkt nach dem Kopf (bei Bytes/Text mit fester Länge: nach dem Inhalt)
};

// Liest den Kopf des Elements an pos. false bei Fehler oder Pufferende.
bool cborHead(const uint8_t *buf, size_t len, size_t pos, CborItem &out);

// Überspringt ein vollständiges Element ab pos. Rückgabe: Position danach, 0 bei Fehler.
size_t cborSkip(const uint8_t *buf, size_t len, size_t pos, int depth = 0);

// Zahl aus UInt, NInt oder Float. false, wenn das Element keine Zahl ist.
bool cborNumber(const CborItem &item, double &out);

// Durchläuft eine Map ab pos. Für jedes Paar mit Integer-Schlüssel wird fn(ctx, key, buf, len, valuePos) aufgerufen.
// Andere Schlüssel werden übersprungen. Rückgabe: Position nach der Map, 0 bei Fehler.
typedef void (*CborMapFn)(void *ctx, int64_t key, const uint8_t *buf, size_t len, size_t valuePos);
size_t cborForEachInMap(const uint8_t *buf, size_t len, size_t pos, CborMapFn fn, void *ctx);

// Schreiber mit fester Puffergröße. ok wird false, sobald etwas nicht passt.
struct CborWriter {
  uint8_t *buf;
  size_t cap;
  size_t pos = 0;
  bool ok = true;
  CborWriter(uint8_t *b, size_t c) : buf(b), cap(c) {}
  void head(uint8_t major, uint64_t value);
  void uint(uint64_t v) { head(0, v); }
  void sint(int64_t v);
  void bytes(const uint8_t *data, size_t n);
  void text(const char *s);
  void mapHeader(size_t pairs) { head(5, pairs); }
  void raw(uint8_t b);
};

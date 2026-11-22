#pragma once
#include <cstddef>
#include <cstdint>

#include "tags.h"

// OpenPrintTag (offener Standard von Prusa Research, MIT-Lizenz, https://specs.openprinttag.org).
// NDEF-MIME-Record "application/vnd.openprinttag", Payload = CBOR: Meta-, Haupt- und optional Aux-Bereich.
// Der Standard ist für NFC-V (ISO 15693, ICODE SLIX2) gedacht. Der PN532 kann kein NFC-V, deshalb liest und
// schreibt die Waage dasselbe Datenformat auf NTAG215/216 (CLAUDE.md Abschnitt 4.7). Reine Funktionen.

#define OPT_MIME_TYPE "application/vnd.openprinttag"

struct OpenPrintTag {
  int materialClass = -1;       // 0 = FFF (Filament), 1 = SLA (Harz), -1 = fehlt
  int materialType = -1;        // Enum-Schlüssel (0 = PLA, 1 = PETG, ...), -1 = fehlt
  char materialName[64] = "";   // z. B. "PLA Prusa Galaxy Black"
  char brand[32] = "";          // brand_name
  bool hasColor = false;
  uint8_t color[3] = {0, 0, 0};  // primary_color RGB (Alpha wird ignoriert)
  int minTemp = 0;               // min_print_temperature, 0 = fehlt
  int maxTemp = 0;               // max_print_temperature, 0 = fehlt
  float nominalWeight = -1;      // nominal_netto_full_weight in g, -1 = fehlt
  float actualWeight = -1;       // actual_netto_full_weight in g
  float emptyContainerWeight = -1;  // empty_container_weight in g
  bool hasAux = false;           // Aux-Bereich vorhanden
  float consumedWeight = -1;     // consumed_weight (Aux) in g
};

// Liest die NDEF-Payload. false, wenn Meta- oder Hauptbereich fehlen bzw. ungültig sind.
bool optParse(const uint8_t *payload, size_t len, OpenPrintTag &out);

// Baut das komplette NDEF-TLV (wie ndefBuildMimeTlv) mit Meta-, Haupt- und einem leeren Aux-Bereich (32 Byte,
// an 4 Byte ausgerichtet). Rückgabe: Bytes in out, 0 wenn es nicht passt.
size_t optBuildTlv(const OpenPrintTag &tag, size_t capacityBytes, uint8_t *out, size_t outCap);

// Abkürzung zum Enum-Schlüssel ("PLA" -> 0), ohne Groß-/Kleinschreibung. nullptr bzw. -1 wenn unbekannt.
const char *optMaterialTypeName(int key);
int optMaterialTypeKey(const char *abbreviation);

// Umwandlung in das Spulenformat der Waage und zurück.
void optToSpool(const OpenPrintTag &tag, SpoolTag &spool);
void optFromSpool(const SpoolTag &spool, OpenPrintTag &tag);

// Kurzbeschreibung als JSON für Anzeige und Weboberfläche, z. B. {"format":"openprinttag","type":"PLA",...}.
size_t optDescribeJson(const OpenPrintTag &tag, char *out, size_t cap);

#pragma once
#include <cstddef>
#include <cstdint>

// Spulen- und Box-Tags (CLAUDE.md Abschnitt 4.2 bis 4.4).
enum class TagKind : uint8_t { Unknown, Spool, Box, User, Admin };

struct SpoolTag {
  char type[16] = "";
  char colorHex[8] = "";
  char brand[32] = "";
  int minTemp = 0;
  int maxTemp = 0;
  int spoolmanId = -1;  // -1 = nicht auf dem Tag
};

struct BoxTag {
  char id[16] = "";
  char name[32] = "";
};

struct UserTag {
  int id = -1;
  char name[24] = "";
  char token[33] = "";  // geheimer Wert pro Benutzer, leer bei Tags im alten Format (nur ID und Name)
};

struct AdminTag {
  char token[33] = "";  // geheimer Wert, den das Gerät mit dem gespeicherten Admin-Token vergleicht
};

struct TagContent {
  TagKind kind = TagKind::Unknown;
  bool openPrintTag = false;  // Spule im OpenPrintTag-Format (CBOR, siehe openprinttag.h) statt OpenSpool-JSON
  SpoolTag spool;
  BoxTag box;
  UserTag user;
  AdminTag admin;
};

// Erkennung: "protocol":"openspool" -> Spule, "type":"box" -> Box, "type":"user" -> Benutzer, "type":"admin" -> Admin, sonst Unknown.
TagContent tagParse(const char *json, size_t len);

// JSON für einen Box-Tag. Rückgabe: Länge, oder 0 wenn es nicht in cap passt.
size_t tagBuildBox(const char *id, const char *name, char *out, size_t cap);

// Box-Tag aus einer Nummer (1 bis 99): id "BOX-07", name "Box 07". Rückgabe wie tagBuildBox, 0 bei ungültiger Nummer.
size_t tagBuildBoxNumber(int number, char *out, size_t cap);

// true, wenn ein gelesener Tag beim Box-Schritt als "leer/unbekannt" gilt und als neue Box beschrieben werden darf:
// kein bekanntes Format (kein NDEF, kein gültiges JSON) oder ein Box-Tag ohne Namen. Spulen-, Benutzer- und
// Admin-Tags zählen nicht.
bool tagIsBlankForBox(const TagContent &tag);

// JSON für einen OpenSpool-Tag (Temperaturen als Strings, wie im Format üblich). spoolmanId < 0 wird weggelassen.
size_t tagBuildSpool(const SpoolTag &spool, char *out, size_t cap);

// JSON für einen Benutzer-Tag: {"type":"user","id":3,"name":"Fabian","token":"<32 Hex-Zeichen>"}
size_t tagBuildUser(int id, const char *name, const char *token, char *out, size_t cap);

// Prüfung des Tokens auf einem Benutzer-Tag gegen das gespeicherte Token des Benutzers.
// Ok: beide gesetzt und gleich. Outdated: Tag ohne Token (altes Format, muss neu geschrieben werden).
// Invalid: Token falsch oder der Benutzer hat noch kein Token (Tag gefälscht oder durch einen neueren ersetzt).
enum class UserTagCheck : uint8_t { Ok, Outdated, Invalid };
UserTagCheck tagCheckUserToken(const char *stored, const char *presented);

// JSON für einen Admin-Tag: {"type":"admin","token":"<32 Hex-Zeichen>"}
size_t tagBuildAdmin(const char *token, char *out, size_t cap);

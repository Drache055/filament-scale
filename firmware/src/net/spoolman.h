#pragma once
#include <Arduino.h>

// Spoolman-Client (API v1, getestet gegen 0.27.0). Alle Anfragen laufen asynchron im eigenen Task,
// Ergebnisse werden per Schnappschuss abgeholt. Spulen werden über die nativen NFC-Tags von Spoolman
// gefunden (GET /spool?tag=UID), das Extra-Feld "nfc_uid" aus früheren Entwürfen wird nicht gebraucht.
struct SpoolInfo {
  int id = -1;
  char vendor[32] = "";
  char material[24] = "";
  char name[32] = "";
  char colorHex[10] = "";
  char location[65] = "";
  char owner[24] = "";
  float remaining = -1;      // g, -1 = unbekannt
  float initialWeight = -1;  // Nettogewicht der vollen Spule
  float emptySpool = -1;     // Leerspulengewicht: Spule, sonst Filament, sonst Hersteller (-1 = unbekannt)
  float filamentWeight = -1; // Füllgewicht der Filamentsorte (nötig für remaining_weight)
  float density = 0;
  float diameter = 0;
  bool archived = false;
};

enum class SpoolmanState : uint8_t { Idle, Busy, Found, NotFound, Error };

struct SpoolmanResult {
  SpoolmanState state = SpoolmanState::Idle;
  uint32_t seq = 0;       // zählt mit jedem abgeschlossenen Auftrag hoch
  char uid[21] = "";      // UID, auf die sich das Ergebnis bezieht (Lookup)
  int httpStatus = 0;
  bool queued = false;    // Update war nicht möglich und wurde für später gespeichert (gilt als erledigt)
  char error[96] = "";
  SpoolInfo spool;
};

struct SpoolmanStatus {
  bool reachable = false;
  char version[16] = "";
  char url[96] = "";
  uint32_t checkedSecondsAgo = 0;
  bool everChecked = false;
  int httpCode = 0;  // HTTP-Status der letzten Prüfung (0 = keine Verbindung)
};

void spoolmanBegin();
String spoolmanUrl();
bool spoolmanSetUrl(const char *url, String &error);  // prüft, speichert (NVS), löst eine Prüfung aus
void spoolmanCheckNow();
SpoolmanStatus spoolmanStatus();

// Spule zu einem Tag suchen: erst über spoolman_id (falls >= 0), sonst über die UID.
void spoolmanRequestLookup(const char *uid, int spoolmanId);
SpoolmanResult spoolmanLookupResult();

// Spule aktualisieren. remaining < 0 = nicht ändern, location/owner nullptr = nicht ändern.
// owner wird im Extra-Feld "owner" gespeichert (das Feld wird bei Bedarf angelegt, andere Extra-Felder bleiben erhalten).
void spoolmanRequestUpdate(int spoolId, float remaining, const char *location, const char *owner);
// Tag (UID) mit der Spule verknüpfen.
void spoolmanRequestLinkTag(int spoolId, const char *uid);
SpoolmanResult spoolmanWriteResult();  // Ergebnis des letzten Update-/Link-Auftrags

// Offline-Warteschlange (siehe storage/queue): Updates, die wegen fehlendem WLAN oder Serverfehler scheitern,
// werden gespeichert und automatisch nachgeholt, sobald Spoolman wieder erreichbar ist.
int spoolmanQueueCount();
uint32_t spoolmanQueueDropped();
void spoolmanQueueFlushNow();   // sofort einen Versuch starten
void spoolmanQueueClear();      // Warteschlange verwerfen

// Liste der Spulen (nicht archiviert) für die Zuordnung eines unbekannten Tags. Höchstens SPOOL_LIST_MAX Einträge,
// "total" ist die Gesamtzahl in Spoolman.
#define SPOOL_LIST_MAX 60
struct SpoolListItem {
  int id = -1;
  char label[48] = "";   // "Hersteller Material"
  float remaining = -1;
};
struct SpoolListResult {
  SpoolmanState state = SpoolmanState::Idle;
  uint32_t seq = 0;
  int count = 0;
  int total = 0;
  char error[96] = "";
  SpoolListItem items[SPOOL_LIST_MAX];
};
void spoolmanRequestList();
void spoolmanListResult(SpoolListResult &out);

// Katalogabgleich: Hersteller (GET /vendor) und Materialien samt Extruder-Temperatur (GET /filament) aus Spoolman in den
// Katalog (storage/catalog) übernehmen. Es wird nur ergänzt, nichts gelöscht oder überschrieben. Läuft automatisch alle
// CATALOG_SYNC_MS, sobald Spoolman erreichbar ist, und auf Anforderung.
struct CatalogSyncResult {
  SpoolmanState state = SpoolmanState::Idle;  // Idle = noch nicht gelaufen, Busy, Found = ok, Error
  uint32_t seq = 0;
  int vendorsRead = 0;
  int materialsRead = 0;
  int addedMakers = 0;
  int addedMaterials = 0;
  int skipped = 0;     // passte nicht (Name zu lang, Liste voll)
  uint32_t secondsAgo = 0;
  char error[96] = "";
};
void spoolmanRequestCatalogSync();
CatalogSyncResult spoolmanCatalogSyncResult();

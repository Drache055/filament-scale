#pragma once
#include <string>
#include <vector>

// Trocknungs-Erinnerung (CLAUDE.md 7.8). Reine Logik ohne Hardwarezugriff, nativ getestet.
// Pro Material gilt ein Intervall in Tagen (0 = keine Erinnerung). Liegt die Spule in einer Box (luftdicht),
// wird das Intervall mit dem Box-Faktor multipliziert. "Zuletzt getrocknet" steht als Datum JJJJ-MM-TT an der Spule.
#define DRYING_MAX_MATERIALS 24
#define DRYING_MAX_NAME 15
#define DRYING_MAX_DAYS 3650
#define DRYING_MAX_FACTOR 10

struct DryingMaterial {
  std::string name;
  int days = 0;
};

struct DryingConfig {
  bool enabled = true;
  int boxFactor = 3;      // 1 bis 10, 1 = Box verlängert das Intervall nicht
  int defaultDays = 0;    // für Materialien ohne eigenen Eintrag, 0 = keine Erinnerung
  std::vector<DryingMaterial> materials;
};

DryingConfig dryingDefaults();

// Intervall in Tagen für ein Material (ohne Box-Faktor). Namensvergleich ohne Groß-/Kleinschreibung.
int dryingBaseDays(const DryingConfig &cfg, const char *material);
// Intervall mit Box-Faktor; 0, wenn die Erinnerung aus ist.
int dryingEffectiveDays(const DryingConfig &cfg, const char *material, bool inBox);

// Tageszahl seit 1970-01-01 (gregorianisch) und Datum "JJJJ-MM-TT".
long dryingDayNumber(int year, int month, int day);
bool dryingParseDate(const char *text, long &dayNumber);
void dryingFormatDate(long dayNumber, char *out11);  // out11: mindestens 11 Zeichen

enum class DryingState { Off, NoRecord, Ok, Due };
struct DryingStatus {
  DryingState state = DryingState::Off;
  int intervalDays = 0;  // wirksames Intervall
  int daysLeft = 0;      // bis zur nächsten Trocknung, <= 0 = überfällig
};

// today < 0 = Uhr nicht gestellt: dann gibt es keine Aussage (Off).
// Off = Erinnerung aus, Material ohne Intervall oder Uhr unbekannt. NoRecord = nie getrocknet eingetragen (keine Warnung).
DryingStatus dryingStatus(const DryingConfig &cfg, const char *material, bool inBox, const char *lastDried, long today);

// Einstellungen als JSON: {"enabled":true,"box_factor":3,"default_days":0,"materials":{"PETG":30}}
std::string dryingConfigToJson(const DryingConfig &cfg);
// Prüft und übernimmt; bei ungültigen Werten false und Fehlertext. cfg bleibt dann unverändert.
bool dryingConfigFromJson(const std::string &json, DryingConfig &cfg, std::string *error = nullptr);

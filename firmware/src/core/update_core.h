#pragma once
#include <cstddef>
#include <cstdint>

// Online-Update (CLAUDE.md 12.6): Versionsvergleich und Auswertung der GitHub-Release. Reine Logik ohne Hardwarezugriff.

// Firmware-Version aus dem Git-Tag, z. B. "v1.2", "v1.2.3", "v1.2-beta" (Pre-Release)
// oder "v1.2-3-gabc1234[-dirty]" (git describe: Entwicklungsstand nach dem Tag).
struct FwVersion {
  bool valid = false;
  int major = 0;
  int minor = 0;
  int patch = 0;
  bool prerelease = false;  // Bindestrich-Zusatz wie "-beta"
  int ahead = 0;            // Commits nach dem Tag (git describe), "-dirty" zählt als einer
};

FwVersion fwParseVersion(const char *s);

// < 0: a älter als b, 0: gleich, > 0: a neuer. Ungültige Versionen gelten als älteste.
int fwCompareVersions(const FwVersion &a, const FwVersion &b);

// Ist latest (stabile Release) neuer als die laufende Version? Bei unbekannter eigener Version: ja.
bool fwUpdateAvailable(const char *latestTag, const char *currentVersion);

#define FW_TAG_LEN   32
#define FW_URL_LEN   320
#define FW_NOTES_LEN 3000  // Release Notes werden auf so viele Bytes gekürzt (an einer UTF-8-Grenze)

struct FwRelease {
  char tag[FW_TAG_LEN] = "";
  char otaUrl[FW_URL_LEN] = "";   // browser_download_url des Assets "*-ota.bin"
  uint32_t otaSize = 0;
  char pageUrl[FW_URL_LEN] = "";  // html_url der Release
  char notes[FW_NOTES_LEN + 1] = "";
  bool notesCut = false;
};

// Antwort von GET /repos/<owner>/<repo>/releases/latest auswerten.
// false mit error, wenn das JSON kaputt ist, es ein Entwurf/Pre-Release ist oder kein passendes OTA-Asset hat.
bool fwParseRelease(const char *json, size_t len, FwRelease &out, const char *&error);

// Ist url ein erlaubter Download-Link (https, github.com bzw. *.githubusercontent.com)?
bool fwAllowedUrl(const char *url);

#pragma once
#include <cstddef>
#include <cstdint>

// Online-Update (CLAUDE.md 12.6): Versionsvergleich und Auswertung der GitHub-Release. Reine Logik ohne Hardwarezugriff.

// Update-Kanäle. Jeder Kanal enthält auch die stabileren: Test sieht alles, Release-Kandidat RC und Stable.
//   Stable: Tag ohne Bindestrich (v1.2), keine Pre-Release auf GitHub
//   Rc:     Release-Kandidat, Tag mit "-rc" (v1.2-rc1)
//   Test:   jeder andere Bindestrich-Zusatz (v1.2-beta1, v1.2-test3)
enum class FwChannel : uint8_t { Stable = 0, Rc = 1, Test = 2 };

// Firmware-Version aus dem Git-Tag, z. B. "v1.2", "v1.2.3", "v1.2-rc1", "v1.2-beta2" (Pre-Release)
// oder "v1.2-3-gabc1234[-dirty]" (git describe: Entwicklungsstand nach dem Tag).
struct FwVersion {
  bool valid = false;
  int major = 0;
  int minor = 0;
  int patch = 0;
  bool prerelease = false;  // Bindestrich-Zusatz wie "-rc1" oder "-beta"
  bool rc = false;          // Zusatz beginnt mit "rc" (Release-Kandidat)
  int preNum = 0;           // Zahl am Ende des Zusatzes ("rc2" -> 2)
  int ahead = 0;            // Commits nach dem Tag (git describe), "-dirty" zählt als einer
};

FwVersion fwParseVersion(const char *s);

// < 0: a älter als b, 0: gleich, > 0: a neuer. Ungültige Versionen gelten als älteste.
// Gleiche Nummer: Test < Release-Kandidat < Stable, innerhalb davon nach preNum (v1.2-beta2 < v1.2-rc1 < v1.2).
int fwCompareVersions(const FwVersion &a, const FwVersion &b);

// Kanal, zu dem eine Release gehört (ghPrerelease: auf GitHub als Pre-Release markiert)
FwChannel fwReleaseChannel(const char *tag, bool ghPrerelease);

// Ist latest neuer als die laufende Version? Bei unbekannter eigener Version: ja.
bool fwUpdateAvailable(const char *latestTag, const char *currentVersion);

// Wäre latest ein Rückschritt (älter als die laufende Version, z. B. beim Wechsel von Test auf Stable)?
bool fwIsDowngrade(const char *latestTag, const char *currentVersion);

const char *fwChannelName(FwChannel ch);  // "stable", "rc", "test"
bool fwChannelFromName(const char *name, FwChannel &out);

#define FW_TAG_LEN   32
#define FW_URL_LEN   320
#define FW_NOTES_LEN 3000  // Release Notes werden auf so viele Bytes gekürzt (an einer UTF-8-Grenze)

struct FwRelease {
  char tag[FW_TAG_LEN] = "";
  FwChannel channel = FwChannel::Stable;
  char otaUrl[FW_URL_LEN] = "";   // browser_download_url des Assets "*-ota.bin"
  uint32_t otaSize = 0;
  char pageUrl[FW_URL_LEN] = "";  // html_url der Release
  char notes[FW_NOTES_LEN + 1] = "";
  bool notesCut = false;
};

// Antwort von GET /repos/<owner>/<repo>/releases auswerten (Liste, neueste zuerst) und die höchste Version wählen,
// die der Kanal erlaubt. Entwürfe, Tags ohne Versionsnummer und Releases ohne OTA-Asset werden übersprungen.
// true und found=false: keine passende Release. false: JSON kaputt (error gesetzt).
bool fwParseReleases(const char *json, size_t len, FwChannel channel, FwRelease &out, bool &found, const char *&error);

// Ist url ein erlaubter Download-Link (https, github.com bzw. *.githubusercontent.com)?
bool fwAllowedUrl(const char *url);

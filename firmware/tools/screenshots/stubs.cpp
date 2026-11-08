// Ersatz für Hardware- und Netzmodule: liefert feste Werte aus `mock`, damit die echten Screens gerendert werden können.
#include "mock.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "bus/i2c_bus.h"
#include "display/display.h"
#include "net/wifi.h"
#include "storage/admin.h"
#include "storage/users.h"
#include "util/log.h"

MockState mock;

uint32_t millis() { return mock.now; }
size_t strlcpy(char *dst, const char *src, size_t size) {
  size_t n = strlen(src);
  if (size) {
    size_t c = n >= size ? size - 1 : n;
    memcpy(dst, src, c);
    dst[c] = 0;
  }
  return n;
}

LittleFSClass LittleFS;
File LittleFSClass::open(const char *path, const char *) {
  std::string p = std::string(DATA_DIR) + path;
  return File(fopen(p.c_str(), "r"));
}

void logPrintf(const char *, ...) {}
void logPrintln(const char *) {}

// display
uint8_t displayGetVcoms() { return mock.vcoms; }
void displaySetVcoms(uint8_t v, bool) { mock.vcoms = v; }
void displayWake() {}
void displayPowerTick() {}
void displaySetScreensaver(uint8_t, uint8_t, bool) {}
void displayGetScreensaver(uint8_t &d, uint8_t &o) { d = 5; o = 15; }
bool displayAsleep() { return false; }
uint32_t displayTouchReleaseCount() { return 0; }
void displayTouchLastRaw(uint16_t &x, uint16_t &y) { x = y = 0; }
void displayTouchSetCalibration(float, float, float, float, bool) {}
void displayTouchRequestReset() {}
bool displayTouchCalibrated() { return true; }
void displaySetBacklight(uint8_t) {}
bool displayBegin() { return true; }

// i18n-Zustand liegt im echten i18n.cpp (Preferences). Dort wird nur die Sprache gebraucht, siehe i18n_stub.cpp.

// scale / nfc
ScaleReading scaleGet() { return mock.scale; }
bool scaleTare() { return true; }
bool scaleCalibrate(float) { return true; }
NfcState nfcGet() { return mock.nfc; }
bool nfcRequestWrite(const char *, NfcWriteOrigin) { return true; }
NfcWriteOrigin nfcWriteOrigin() { return NfcWriteOrigin::Device; }
void nfcAcknowledgeWrite() {}
NfcWrite nfcWriteStatus() { return mock.nfcWrite; }
void nfcCancelWrite() {}

// wifi
bool wifiConnected() { return mock.wifiUp; }
String wifiSsid() { return "Heimnetz"; }
String wifiIp() { return "192.168.178.57"; }
int wifiRssi() { return -58; }
bool wifiApActive() { return false; }
String wifiApSsid() { return "Filamentwaage-Setup"; }
String wifiApIp() { return "192.168.4.1"; }

// spoolman
SpoolmanStatus spoolmanStatus() { return mock.spoolman; }
SpoolmanResult spoolmanLookupResult() { return mock.lookup; }
SpoolmanResult spoolmanWriteResult() { return mock.write; }
void spoolmanRequestLookup(const char *, int) {}
void spoolmanRequestUpdate(int, float, const char *, const char *) {}
void spoolmanRequestLinkTag(int, const char *) {}
int spoolmanQueueCount() { return mock.queued; }
void spoolmanRequestList() {}
void spoolmanListResult(SpoolListResult &out) { out = mock.list; }

// storage
void catalogGet(Catalog &out) {
  const char *types[] = {"PLA", "ABS", "PETG"};
  int lo[] = {190, 230, 220}, hi[] = {220, 260, 250};
  out.materialCount = 3;
  for (int i = 0; i < 3; i++) {
    strlcpy(out.materials[i].name, types[i], sizeof(out.materials[i].name));
    out.materials[i].minTemp = lo[i];
    out.materials[i].maxTemp = hi[i];
  }
  out.makerCount = 2;
  strlcpy(out.makers[0], "Elegoo", 32);
  strlcpy(out.makers[1], "Generic", 32);
  const char *cn[] = {"Weiß", "Schwarz", "Rot", "Blau", "Grün", "Gelb"};
  const char *ch[] = {"FFFFFF", "000000", "E11D2A", "2563EB", "16A34A", "FACC15"};
  out.colorCount = 6;
  for (int i = 0; i < 6; i++) {
    strlcpy(out.colors[i].name, cn[i], sizeof(out.colors[i].name));
    strlcpy(out.colors[i].hex, ch[i], sizeof(out.colors[i].hex));
  }
}

bool adminLockEnabled() { return mock.lockEnabled; }
bool adminSessionActive() { return mock.adminLoggedIn; }
bool adminAllowed() { return !mock.lockEnabled || mock.adminLoggedIn; }
void adminSessionStart() { mock.adminLoggedIn = true; }
void adminSessionEnd() { mock.adminLoggedIn = false; }
void adminSessionTouch() {}
bool adminCheckDevicePin(const char *) { return false; }

// session
bool sessionCurrentUser(User &out) {
  if (!mock.userLoggedIn) return false;
  out.id = 3;
  strlcpy(out.name, "Fabian", sizeof(out.name));
  return true;
}
bool sessionUserMayTare() { return false; }
void sessionLogout() { mock.userLoggedIn = false; }
bool sessionSetMyLanguage(Lang) { return true; }
void sessionUserChanged(uint16_t) {}
bool sessionNotice(char *, size_t, int *) { return false; }
void sessionPostNotice(const char *, int) {}

// i2c
void i2cBusBegin() {}
bool i2cBusLock(uint32_t) { return true; }
void i2cBusUnlock() {}

#include "nfc.h"
#include "util/log.h"

#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <freertos/semphr.h>

#include "pins.h"
#include "config.h"
#include "ndef.h"
#include "openprinttag.h"

namespace {

// PN532 per Software-SPI an eigenen Pins, getrennt vom Touch-I²C-Bus: kein Mutex nötig, der Touch wird nie blockiert.
Adafruit_PN532 pn532(PIN_NFC_SCK, PIN_NFC_MISO, PIN_NFC_MOSI, PIN_NFC_SS);

SemaphoreHandle_t stateMutex = nullptr;
NfcState state;

// Schreibauftrag: JSON-Record oder OpenPrintTag
char writeJson[512];
bool writeOpt = false;
OpenPrintTag writeOptTag;
volatile NfcWrite writeStatus = NfcWrite::Idle;
volatile NfcWriteOrigin writeOrigin = NfcWriteOrigin::Device;
uint32_t writeSeq = 0;  // state.seq beim Auftrag: Web-Aufträge brauchen eine spätere Erkennung
uint32_t writeDeadline = 0;

constexpr size_t MAX_NDEF_BYTES = 900;

// ---- Seitenzugriff, jeweils kurz unter dem I²C-Mutex, damit der Touch nicht blockiert ----
bool readPage(uint8_t page, uint8_t *buf4) {
  return pn532.ntag2xx_ReadPage(page, buf4) != 0;
}

bool writePage(uint8_t page, uint8_t *buf4) {
  return pn532.ntag2xx_WritePage(page, buf4) != 0;
}

bool readBytes(uint8_t firstPage, uint8_t *dst, size_t bytes) {
  for (size_t i = 0; i < bytes; i += 4) {
    if (!readPage(firstPage + i / 4, dst + i)) {
      return false;
    }
  }
  return true;
}

// Schreibschutz: Lock-Bytes (Page 2), dynamische Lock-Bytes, CC
bool tagIsLocked(const NdefCc &cc) {
  if (cc.readOnly) {
    return true;
  }
  uint8_t p[4];
  if (!readPage(2, p)) {
    return true;  // im Zweifel nicht schreiben
  }
  if ((p[2] & 0xF0) != 0 || p[3] != 0) {  // L4..L15 (Pages 4 bis 15)
    return true;
  }
  uint8_t dynPage = 0;
  if (cc.capacityBytes == 496) {
    dynPage = 0x82;  // NTAG215
  } else if (cc.capacityBytes == 872) {
    dynPage = 0xE2;  // NTAG216
  }
  if (dynPage) {
    if (!readPage(dynPage, p)) {
      return true;
    }
    if (p[0] || p[1] || p[2]) {
      return true;
    }
  }
  return false;
}

// Liest CC und NDEF des aufliegenden Tags in den Zustand. false = Lesefehler (Tag bewegt), dann später neu versuchen.
bool readTag() {
  NfcState next;
  {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    next = state;
    xSemaphoreGive(stateMutex);
  }
  next.capacity = 0;
  next.locked = false;
  next.hasNdef = false;
  next.json[0] = '\0';
  next.content = TagContent();

  uint8_t ccRaw[4];
  if (!readPage(3, ccRaw)) {
    return false;
  }
  NdefCc cc;
  if (ndefParseCc(ccRaw, cc)) {
    next.capacity = cc.capacityBytes;
    static uint8_t buf[MAX_NDEF_BYTES + 16];
    size_t have = 32;
    if (!readBytes(4, buf, have)) {
      return false;
    }
    NdefLocation loc;
    NdefLocate r = ndefLocate(buf, have, loc);
    while (r == NdefLocate::NeedMore) {
      size_t want = min((size_t)(((loc.neededBytes + 3) & ~(size_t)3)), min(cc.capacityBytes, MAX_NDEF_BYTES));
      if (want <= have) {
        r = NdefLocate::Invalid;
        break;
      }
      if (!readBytes(4 + have / 4, buf + have, want - have)) {
        return false;
      }
      have = want;
      r = ndefLocate(buf, have, loc);
    }
    if (r == NdefLocate::Found) {
      const uint8_t *msg = buf + loc.recordOffset;
      NdefMime mime;
      OpenPrintTag opt;
      if (ndefFindMimeRecord(msg, loc.recordLength, "application/json", mime)) {
        next.hasNdef = true;
        size_t n = min(mime.payloadLength, sizeof(next.json) - 1);
        memcpy(next.json, mime.payload, n);
        next.json[n] = '\0';
        next.content = tagParse((const char *)mime.payload, mime.payloadLength);
      } else if (ndefFindMimeRecord(msg, loc.recordLength, OPT_MIME_TYPE, mime) &&
                 optParse(mime.payload, mime.payloadLength, opt)) {
        next.hasNdef = true;
        optDescribeJson(opt, next.json, sizeof(next.json));
        next.content.kind = TagKind::Spool;
        next.content.openPrintTag = true;
        optToSpool(opt, next.content.spool);
      }
    }
    next.locked = tagIsLocked(cc);
  }

  next.readSeq = next.seq;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  state = next;
  xSemaphoreGive(stateMutex);
  return true;
}

// ---- Schreiben ----
NfcWrite doWrite() {
  uint8_t ccRaw[4];
  if (!readPage(3, ccRaw)) {
    return NfcWrite::ErrFailed;
  }
  NdefCc cc;
  if (!ndefParseCc(ccRaw, cc)) {
    return NfcWrite::ErrNotNtag;
  }
  static uint8_t out[MAX_NDEF_BYTES];
  size_t len = writeOpt ? optBuildTlv(writeOptTag, cc.capacityBytes, out, sizeof(out))
                        : ndefBuildMimeTlv("application/json", (const uint8_t *)writeJson, strlen(writeJson),
                                           cc.capacityBytes, out, sizeof(out));
  if (len == 0) {
    return NfcWrite::ErrTooBig;
  }
  if (tagIsLocked(cc)) {
    return NfcWrite::ErrLocked;
  }
  for (size_t i = 0; i < len; i += 4) {
    if (!writePage(4 + i / 4, out + i)) {
      return NfcWrite::ErrFailed;
    }
  }
  // Gegenprobe
  static uint8_t check[MAX_NDEF_BYTES];
  if (!readBytes(4, check, len) || memcmp(check, out, len) != 0) {
    return NfcWrite::ErrFailed;
  }
  logPrintf("[NFC] %u Bytes geschrieben\n", (unsigned)len);
  return NfcWrite::Ok;
}

bool initModule() {
  bool ok = false;
  if (pn532.begin()) {
    uint32_t version = pn532.getFirmwareVersion();
    if (!version) {
      logPrintln("[NFC] Firmware-Abfrage fehlgeschlagen");
    }
    if (version) {
      logPrintf("[NFC] PN5%02X, Firmware %u.%u\n", (unsigned)(version >> 24), (unsigned)((version >> 16) & 0xFF),
                    (unsigned)((version >> 8) & 0xFF));
      pn532.SAMConfig();
      pn532.setPassiveActivationRetries(0x02);  // der Chip meldet "kein Tag" selbst schnell zurück
      ok = true;
    }
  }
  return ok;
}

void setPresent(bool present, const char *uid) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  state.tagPresent = present;
  if (present) {
    strncpy(state.uid, uid, sizeof(state.uid) - 1);
    state.seq++;
  } else {
    state.uid[0] = '\0';
    state.capacity = 0;
    state.locked = false;
    state.hasNdef = false;
    state.json[0] = '\0';
    state.content = TagContent();
  }
  xSemaphoreGive(stateMutex);
}

void nfcTask(void *) {
  uint32_t nextInit = 0;
  int misses = 0;
  char currentUid[21] = "";
  bool needRead = false;

  for (;;) {
    if (!state.moduleReady) {
      if (millis() >= nextInit) {
        bool ok = initModule();
        nextInit = millis() + NFC_INIT_RETRY_MS;
        if (ok) {
          xSemaphoreTake(stateMutex, portMAX_DELAY);
          state.moduleReady = true;
          xSemaphoreGive(stateMutex);
        }
      }
      if (writeStatus == NfcWrite::WaitingForTag && millis() > writeDeadline) {
        writeStatus = NfcWrite::ErrTimeout;
      }
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    uint8_t uid[10];
    uint8_t uidLen = 0;
    bool found = false;
    found = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50);

    if (found && uidLen > 0 && uidLen <= 10) {
      misses = 0;
      char hex[21];
      for (uint8_t i = 0; i < uidLen; i++) {
        snprintf(hex + i * 2, 3, "%02X", uid[i]);
      }
      if (strcmp(hex, currentUid) != 0) {
        strcpy(currentUid, hex);
        setPresent(true, hex);
        needRead = true;
        logPrintf("[NFC] Tag erkannt: %s\n", hex);
      }
      if (needRead) {
        needRead = !readTag();  // bei Lesefehler im nächsten Durchgang erneut
        if (!needRead) {
          NfcState s = nfcGet();
          logPrintf("[NFC] Kapazität %u, %s, Inhalt: %s\n", (unsigned)s.capacity, s.locked ? "gesperrt" : "frei",
                        s.hasNdef ? s.json : "(kein NDEF)");
        }
      }
      bool seqOk = writeOrigin == NfcWriteOrigin::Device || nfcGet().seq > writeSeq;
      if (writeStatus == NfcWrite::WaitingForTag && !needRead && seqOk) {
        writeStatus = NfcWrite::Writing;
        NfcWrite result = (writeOrigin == NfcWriteOrigin::Web && nfcGet().content.kind == TagKind::Spool)
                              ? NfcWrite::ErrOccupied
                              : doWrite();
        writeStatus = result;
        if (result == NfcWrite::Ok) {
          readTag();
        } else {
          logPrintf("[NFC] Schreiben fehlgeschlagen (%d)\n", (int)result);
        }
      }
    } else if (!found) {
      if (++misses >= NFC_MISSES_FOR_GONE && currentUid[0]) {
        currentUid[0] = '\0';
        setPresent(false, "");
        logPrintln("[NFC] Tag entfernt");
      }
    }

    if (writeStatus == NfcWrite::WaitingForTag && millis() > writeDeadline) {
      writeStatus = NfcWrite::ErrTimeout;
    }
    vTaskDelay(pdMS_TO_TICKS(NFC_POLL_MS));
  }
}

}  // namespace

void nfcBegin() {
  stateMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(nfcTask, "nfc", 6144, nullptr, 2, nullptr, 1);
}

NfcState nfcGet() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  NfcState copy = state;
  xSemaphoreGive(stateMutex);
  return copy;
}

bool nfcRequestWrite(const char *json, NfcWriteOrigin origin) {
  if (writeStatus == NfcWrite::WaitingForTag || writeStatus == NfcWrite::Writing || strlen(json) >= sizeof(writeJson)) {
    return false;
  }
  strcpy(writeJson, json);
  writeOpt = false;
  writeOrigin = origin;
  writeSeq = nfcGet().seq;
  writeDeadline = millis() + NFC_WRITE_TIMEOUT_MS;
  writeStatus = NfcWrite::WaitingForTag;
  return true;
}

bool nfcRequestWriteOpenPrintTag(const SpoolTag &spool) {
  if (writeStatus == NfcWrite::WaitingForTag || writeStatus == NfcWrite::Writing) {
    return false;
  }
  optFromSpool(spool, writeOptTag);
  writeOpt = true;
  writeOrigin = NfcWriteOrigin::Device;
  writeSeq = nfcGet().seq;
  writeDeadline = millis() + NFC_WRITE_TIMEOUT_MS;
  writeStatus = NfcWrite::WaitingForTag;
  return true;
}

NfcWrite nfcWriteStatus() { return writeStatus; }

void nfcCancelWrite() {
  if (writeStatus != NfcWrite::Writing) {
    writeStatus = NfcWrite::Idle;
  }
}

NfcWriteOrigin nfcWriteOrigin() { return writeOrigin; }

void nfcAcknowledgeWrite() {
  NfcWrite st = writeStatus;
  if (st != NfcWrite::WaitingForTag && st != NfcWrite::Writing) {
    writeStatus = NfcWrite::Idle;
  }
}

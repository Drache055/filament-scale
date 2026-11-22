#include "websession.h"

#include <freertos/semphr.h>

#include "config.h"
#include "util/secret.h"

namespace {

struct Entry {
  bool used = false;
  char token[33] = "";
  WebSession session;
  uint32_t lastMs = 0;
};

constexpr int MAX_SESSIONS = 6;
Entry entries[MAX_SESSIONS];
SemaphoreHandle_t mutex = nullptr;

void ensureMutex() {
  if (!mutex) {
    mutex = xSemaphoreCreateMutex();
  }
}

}  // namespace

String webSessionCreate(WebRole role, uint16_t userId) {
  ensureMutex();
  String token = secretRandomHex(16);
  xSemaphoreTake(mutex, portMAX_DELAY);
  int slot = -1;
  uint32_t now = millis();
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!entries[i].used || now - entries[i].lastMs > WEB_SESSION_MS) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {  // voll: die älteste Sitzung verdrängen
    slot = 0;
    for (int i = 1; i < MAX_SESSIONS; i++) {
      if (now - entries[i].lastMs > now - entries[slot].lastMs) {
        slot = i;
      }
    }
  }
  entries[slot].used = true;
  strlcpy(entries[slot].token, token.c_str(), sizeof(entries[slot].token));
  entries[slot].session.role = role;
  entries[slot].session.userId = userId;
  entries[slot].lastMs = now;
  xSemaphoreGive(mutex);
  return token;
}

bool webSessionGet(const String &token, WebSession &out) {
  if (token.length() != 32) {
    return false;
  }
  ensureMutex();
  bool ok = false;
  uint32_t now = millis();
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (entries[i].used && secretEqual(token, String(entries[i].token))) {
      if (now - entries[i].lastMs > WEB_SESSION_MS) {
        entries[i].used = false;
      } else {
        entries[i].lastMs = now;
        out = entries[i].session;
        ok = true;
      }
      break;
    }
  }
  xSemaphoreGive(mutex);
  return ok;
}

void webSessionDestroy(const String &token) {
  ensureMutex();
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (entries[i].used && secretEqual(token, String(entries[i].token))) {
      entries[i].used = false;
    }
  }
  xSemaphoreGive(mutex);
}

void webSessionsDropUser(uint16_t userId) {
  ensureMutex();
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (entries[i].used && entries[i].session.role == WebRole::User && entries[i].session.userId == userId) {
      entries[i].used = false;
    }
  }
  xSemaphoreGive(mutex);
}

void webSessionsDropAdminExcept(const String &keepToken) {
  ensureMutex();
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (entries[i].used && entries[i].session.role == WebRole::Admin && !secretEqual(keepToken, String(entries[i].token))) {
      entries[i].used = false;
    }
  }
  xSemaphoreGive(mutex);
}

void webSessionsDropAdmin() { webSessionsDropAdminExcept(String()); }

void webSessionsDropGeneric() {
  ensureMutex();
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (entries[i].used && entries[i].session.role == WebRole::Generic) {
      entries[i].used = false;
    }
  }
  xSemaphoreGive(mutex);
}

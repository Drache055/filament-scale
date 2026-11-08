#include "queue.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "util/log.h"

namespace {

SemaphoreHandle_t mutex = nullptr;
uint32_t dropped = 0;

std::vector<QueueEntry> load() {
  File f = LittleFS.open(QUEUE_FILE, "r");
  if (!f) {
    return {};
  }
  std::string text;
  text.reserve(f.size());
  while (f.available()) {
    text += (char)f.read();
  }
  f.close();
  return queueParse(text);
}

bool store(const std::vector<QueueEntry> &list) {
  if (list.empty()) {
    LittleFS.remove(QUEUE_FILE);
    return true;
  }
  File f = LittleFS.open(QUEUE_FILE, "w");
  if (!f) {
    return false;
  }
  std::string text = queueSerialize(list);
  size_t written = f.write((const uint8_t *)text.data(), text.size());
  f.close();
  return written == text.size();
}

}  // namespace

void queueBegin() {
  mutex = xSemaphoreCreateMutex();
  logPrintf("[QUEUE] %d ausstehende Updates\n", queueCount());
}

bool queueAdd(const QueueEntry &e) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  std::vector<QueueEntry> list = load();
  queueAddEntry(list, e, QUEUE_MAX_ENTRIES);
  bool ok = store(list);
  xSemaphoreGive(mutex);
  logPrintf("[QUEUE] Update für Spule %d gespeichert (%s)\n", e.spoolId, ok ? "ok" : "Schreibfehler");
  return ok;
}

int queueCount() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  int n = (int)load().size();
  xSemaphoreGive(mutex);
  return n;
}

bool queuePeek(QueueEntry &out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  std::vector<QueueEntry> list = load();
  bool has = !list.empty();
  if (has) {
    out = list.front();
  }
  xSemaphoreGive(mutex);
  return has;
}

void queuePopFront() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  std::vector<QueueEntry> list = load();
  if (!list.empty()) {
    list.erase(list.begin());
    store(list);
  }
  xSemaphoreGive(mutex);
}

void queueClear() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  LittleFS.remove(QUEUE_FILE);
  xSemaphoreGive(mutex);
}

uint32_t queueDropped() { return dropped; }
void queueCountDropped() { dropped++; }

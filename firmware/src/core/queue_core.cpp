#include "queue_core.h"

#include <ArduinoJson.h>
#include <cstring>

std::string queueEntryToLine(const QueueEntry &e) {
  JsonDocument doc;
  doc["spool_id"] = e.spoolId;
  if (e.remaining >= 0) {
    doc["remaining_weight"] = e.remaining;
  }
  if (e.hasLocation) {
    doc["location"] = e.location;
  }
  if (e.owner[0]) {
    doc["owner"] = e.owner;
  }
  if (e.archive) {
    doc["archive"] = true;
    if (e.tagUid[0]) {
      doc["tag_uid"] = e.tagUid;
    }
  }
  doc["timestamp"] = e.timestamp;
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool queueEntryFromLine(const std::string &line, QueueEntry &out) {
  JsonDocument doc;
  if (deserializeJson(doc, line) || !doc["spool_id"].is<int>()) {
    return false;
  }
  out = QueueEntry();
  out.spoolId = doc["spool_id"];
  if (out.spoolId <= 0) {
    return false;
  }
  out.remaining = doc["remaining_weight"].is<float>() ? doc["remaining_weight"].as<float>() : -1;
  if (doc["location"].is<const char *>()) {
    out.hasLocation = true;
    strncpy(out.location, doc["location"].as<const char *>(), sizeof(out.location) - 1);
  }
  strncpy(out.owner, doc["owner"] | "", sizeof(out.owner) - 1);
  out.archive = doc["archive"] | false;
  strncpy(out.tagUid, doc["tag_uid"] | "", sizeof(out.tagUid) - 1);
  out.timestamp = doc["timestamp"] | 0u;
  return true;
}

std::vector<QueueEntry> queueParse(const std::string &text) {
  std::vector<QueueEntry> list;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    QueueEntry e;
    if (!line.empty() && queueEntryFromLine(line, e)) {
      list.push_back(e);
    }
  }
  return list;
}

std::string queueSerialize(const std::vector<QueueEntry> &list) {
  std::string out;
  for (const QueueEntry &e : list) {
    out += queueEntryToLine(e);
    out += '\n';
  }
  return out;
}

void queueAddEntry(std::vector<QueueEntry> &list, const QueueEntry &e, size_t maxEntries) {
  QueueEntry merged = e;
  for (size_t i = 0; i < list.size();) {
    if (list[i].spoolId == e.spoolId) {
      // Ältere Angaben bleiben erhalten, wo der neue Eintrag nichts setzt.
      const QueueEntry &old = list[i];
      if (merged.remaining < 0) {
        merged.remaining = old.remaining;
      }
      if (!merged.hasLocation && old.hasLocation) {
        merged.hasLocation = true;
        strncpy(merged.location, old.location, sizeof(merged.location) - 1);
      }
      if (!merged.owner[0]) {
        strncpy(merged.owner, old.owner, sizeof(merged.owner) - 1);
      }
      if (old.archive) {
        merged.archive = true;  // einmal vorgemerkt, bleibt vorgemerkt
      }
      if (!merged.tagUid[0]) {
        strncpy(merged.tagUid, old.tagUid, sizeof(merged.tagUid) - 1);
      }
      list.erase(list.begin() + i);
    } else {
      i++;
    }
  }
  list.push_back(merged);
  while (list.size() > maxEntries) {
    list.erase(list.begin());
  }
}

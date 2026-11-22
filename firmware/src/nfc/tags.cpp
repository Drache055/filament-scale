#include "tags.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

static void copyStr(char *dst, size_t cap, JsonVariantConst v) {
  const char *s = v | "";
  strncpy(dst, s, cap - 1);
  dst[cap - 1] = '\0';
}

TagContent tagParse(const char *json, size_t len) {
  TagContent tag;
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc.is<JsonObject>()) {
    return tag;
  }
  const char *protocol = doc["protocol"] | "";
  const char *type = doc["type"] | "";

  if (strcmp(protocol, "openspool") == 0) {
    tag.kind = TagKind::Spool;
    copyStr(tag.spool.type, sizeof(tag.spool.type), doc["type"]);
    copyStr(tag.spool.colorHex, sizeof(tag.spool.colorHex), doc["color_hex"]);
    copyStr(tag.spool.brand, sizeof(tag.spool.brand), doc["brand"]);
    tag.spool.minTemp = doc["min_temp"].as<int>();  // OpenSpool liefert Strings, as<int> wandelt sie
    tag.spool.maxTemp = doc["max_temp"].as<int>();
    tag.spool.spoolmanId = doc["spoolman_id"].isNull() ? -1 : doc["spoolman_id"].as<int>();
  } else if (strcmp(type, "user") == 0) {
    tag.kind = TagKind::User;
    tag.user.id = doc["id"] | -1;
    copyStr(tag.user.name, sizeof(tag.user.name), doc["name"]);
    copyStr(tag.user.token, sizeof(tag.user.token), doc["token"]);
  } else if (strcmp(type, "admin") == 0) {
    tag.kind = TagKind::Admin;
    copyStr(tag.admin.token, sizeof(tag.admin.token), doc["token"]);
  } else if (strcmp(type, "box") == 0) {
    tag.kind = TagKind::Box;
    copyStr(tag.box.id, sizeof(tag.box.id), doc["id"]);
    copyStr(tag.box.name, sizeof(tag.box.name), doc["name"]);
  }
  return tag;
}

size_t tagBuildBox(const char *id, const char *name, char *out, size_t cap) {
  JsonDocument doc;
  doc["type"] = "box";
  doc["id"] = id;
  doc["name"] = name;
  size_t need = measureJson(doc);
  if (need + 1 > cap) {
    return 0;
  }
  return serializeJson(doc, out, cap);
}

size_t tagBuildBoxNumber(int number, char *out, size_t cap) {
  if (number < 1 || number > 99) {
    return 0;
  }
  char id[16], name[16];
  snprintf(id, sizeof(id), "BOX-%02d", number);
  snprintf(name, sizeof(name), "Box %02d", number);
  return tagBuildBox(id, name, out, cap);
}

bool tagIsBlankForBox(const TagContent &tag) {
  return tag.kind == TagKind::Unknown || (tag.kind == TagKind::Box && tag.box.name[0] == '\0');
}

size_t tagBuildSpool(const SpoolTag &spool, char *out, size_t cap) {
  JsonDocument doc;
  doc["protocol"] = "openspool";
  doc["version"] = "1.0";
  doc["type"] = spool.type;
  doc["color_hex"] = spool.colorHex;
  doc["brand"] = spool.brand;
  char temp[8];
  snprintf(temp, sizeof(temp), "%d", spool.minTemp);
  doc["min_temp"] = temp;  // Temperaturen als Strings, wie im OpenSpool-Format
  char temp2[8];
  snprintf(temp2, sizeof(temp2), "%d", spool.maxTemp);
  doc["max_temp"] = temp2;
  if (spool.spoolmanId >= 0) {
    doc["spoolman_id"] = spool.spoolmanId;
  }
  if (measureJson(doc) + 1 > cap) {
    return 0;
  }
  return serializeJson(doc, out, cap);
}

size_t tagBuildUser(int id, const char *name, const char *token, char *out, size_t cap) {
  JsonDocument doc;
  doc["type"] = "user";
  doc["id"] = id;
  doc["name"] = name;
  doc["token"] = token;
  if (measureJson(doc) + 1 > cap) {
    return 0;
  }
  return serializeJson(doc, out, cap);
}

size_t tagBuildAdmin(const char *token, char *out, size_t cap) {
  JsonDocument doc;
  doc["type"] = "admin";
  doc["token"] = token;
  if (measureJson(doc) + 1 > cap) {
    return 0;
  }
  return serializeJson(doc, out, cap);
}

UserTagCheck tagCheckUserToken(const char *stored, const char *presented) {
  if (!presented || !presented[0]) {
    return UserTagCheck::Outdated;
  }
  if (!stored || !stored[0]) {
    return UserTagCheck::Invalid;
  }
  size_t ls = strlen(stored), lp = strlen(presented);
  if (ls != lp) {
    return UserTagCheck::Invalid;
  }
  // Vergleich ohne frühen Abbruch
  uint8_t diff = 0;
  for (size_t i = 0; i < ls; i++) {
    diff |= (uint8_t)(stored[i] ^ presented[i]);
  }
  return diff == 0 ? UserTagCheck::Ok : UserTagCheck::Invalid;
}

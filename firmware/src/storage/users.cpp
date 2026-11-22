#include "users.h"
#include "util/log.h"
#include "util/secret.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <strings.h>

namespace {

SemaphoreHandle_t mutex = nullptr;
User users[USERS_MAX];
int count = 0;
uint16_t nextId = 1;
bool genericOn = true;

void pinKey(uint16_t id, char *key, size_t cap) { snprintf(key, cap, "pin%u", (unsigned)id); }

// Muss mit gehaltenem Mutex aufgerufen werden
void save() {
  JsonDocument doc;
  doc["next"] = nextId;
  JsonArray arr = doc["users"].to<JsonArray>();
  for (int i = 0; i < count; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = users[i].id;
    o["name"] = users[i].name;
    o["lang"] = users[i].lang;
    o["tare"] = users[i].canTare;
    o["cat"] = users[i].canCatalog;
    o["wo"] = users[i].weighOnly;
  }
  String out;
  serializeJson(doc, out);
  Preferences prefs;
  prefs.begin("users", false);
  prefs.putString("json", out);
  prefs.end();
}

}  // namespace

void usersBegin() {
  mutex = xSemaphoreCreateMutex();
  Preferences prefs;
  prefs.begin("users", false);
  String stored = prefs.getString("json", "");
  prefs.end();
  if (stored.length()) {
    JsonDocument doc;
    if (!deserializeJson(doc, stored)) {
      nextId = doc["next"] | 1;
      for (JsonObjectConst o : doc["users"].as<JsonArrayConst>()) {
        if (count >= USERS_MAX) {
          break;
        }
        users[count].id = o["id"] | 0;
        strlcpy(users[count].name, o["name"] | "", USER_NAME_LEN);
        uint8_t lang = o["lang"] | 0;
        users[count].lang = lang <= 2 ? lang : 0;
        users[count].canTare = o["tare"] | false;
        users[count].canCatalog = o["cat"] | false;
        users[count].weighOnly = o["wo"] | false;
        if (users[count].id && users[count].name[0]) {
          count++;
        }
      }
    } else {
      logPrintln("[USERS] Gespeicherte Benutzerliste ungültig, starte leer");
    }
  }
  {
    Preferences p;
    p.begin("users", false);
    genericOn = p.getBool("gen", true);
    p.end();
  }
  logPrintf("[USERS] %d Benutzer\n", count);
}

int usersList(User *out, int cap) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  int n = min(count, cap);
  for (int i = 0; i < n; i++) {
    out[i] = users[i];
  }
  xSemaphoreGive(mutex);
  return n;
}

bool usersFind(uint16_t id, User &out) {
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      out = users[i];
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersAdd(const char *name, String &error, uint16_t *newId) {
  String n(name ? name : "");
  n.trim();
  if (n.length() == 0 || n.length() >= USER_NAME_LEN) {
    error = String("Name leer oder länger als ") + (USER_NAME_LEN - 1) + " Zeichen";
    return false;
  }
  for (size_t i = 0; i < n.length(); i++) {
    if ((uint8_t)n[i] < 0x20) {
      error = "Name enthält Steuerzeichen";
      return false;
    }
  }
  bool ok = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (count >= USERS_MAX) {
    error = String("maximal ") + USERS_MAX + " Benutzer";
  } else {
    bool duplicate = false;
    for (int i = 0; i < count; i++) {
      if (strcasecmp(users[i].name, n.c_str()) == 0) {
        duplicate = true;
      }
    }
    if (duplicate) {
      error = "Name existiert schon";
    } else {
      users[count].id = nextId++;
      strlcpy(users[count].name, n.c_str(), USER_NAME_LEN);
      if (newId) {
        *newId = users[count].id;
      }
      count++;
      save();
      ok = true;
    }
  }
  xSemaphoreGive(mutex);
  return ok;
}

bool usersRemove(uint16_t id) {
  bool removed = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      for (int j = i; j < count - 1; j++) {
        users[j] = users[j + 1];
      }
      count--;
      save();
      Preferences p;
      char key[12];
      pinKey(id, key, sizeof(key));
      p.begin("users", false);
      p.remove(key);
      p.end();
      removed = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return removed;
}

bool usersSetCatalog(uint16_t id, bool allowed) {
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      users[i].canCatalog = allowed;
      save();
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersSetWeighOnly(uint16_t id, bool on) {
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      users[i].weighOnly = on;
      save();
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersSetTare(uint16_t id, bool allowed) {
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      users[i].canTare = allowed;
      save();
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersSetLang(uint16_t id, uint8_t lang) {
  if (lang > 2) {
    return false;
  }
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (users[i].id == id) {
      users[i].lang = lang;
      save();
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersFindByName(const char *name, User &out) {
  if (!name || !name[0]) {
    return false;
  }
  bool found = false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  for (int i = 0; i < count; i++) {
    if (strcasecmp(users[i].name, name) == 0) {
      out = users[i];
      found = true;
      break;
    }
  }
  xSemaphoreGive(mutex);
  return found;
}

bool usersSetPin(uint16_t id, const char *pin, String &error) {
  User u;
  if (id != 0 && !usersFind(id, u)) {
    error = "Benutzer nicht gefunden";
    return false;
  }
  char key[12];
  pinKey(id, key, sizeof(key));
  Preferences p;
  size_t len = pin ? strlen(pin) : 0;
  if (len == 0) {
    p.begin("users", false);
    p.remove(key);
    p.end();
    return true;
  }
  if (len < 4 || len > 6) {
    error = "PIN: 4 bis 6 Ziffern";
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (pin[i] < '0' || pin[i] > '9') {
      error = "PIN: nur Ziffern";
      return false;
    }
  }
  String salt = secretRandomHex(8);  // 16 Hex-Zeichen
  String stored = salt + secretHash(salt, pin);
  p.begin("users", false);
  p.putString(key, stored);
  p.end();
  return true;
}

bool usersHasPin(uint16_t id) {
  char key[12];
  pinKey(id, key, sizeof(key));
  Preferences p;
  p.begin("users", true);
  bool has = p.getString(key, "").length() == 80;
  p.end();
  return has;
}

bool usersCheckPin(uint16_t id, const char *pin) {
  if (!pin) {
    return false;
  }
  char key[12];
  pinKey(id, key, sizeof(key));
  Preferences p;
  p.begin("users", true);
  String stored = p.getString(key, "");
  p.end();
  if (stored.length() != 80) {
    return false;
  }
  String salt = stored.substring(0, 16);
  return secretEqual(secretHash(salt, pin), stored.substring(16));
}

bool usersGenericEnabled() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = genericOn;
  xSemaphoreGive(mutex);
  return v;
}

void usersSetGenericEnabled(bool on) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  genericOn = on;
  Preferences p;
  p.begin("users", false);
  p.putBool("gen", on);
  p.end();
  xSemaphoreGive(mutex);
}

String usersJson() {
  User list[USERS_MAX];
  int n = usersList(list, USERS_MAX);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = list[i].id;
    o["name"] = list[i].name;
    o["lang"] = list[i].lang == 1 ? "de" : list[i].lang == 2 ? "en" : "";
    o["tare"] = list[i].canTare;
    o["catalog"] = list[i].canCatalog;
    o["weigh_only"] = list[i].weighOnly;
    o["pin"] = usersHasPin(list[i].id);
  }
  String out;
  serializeJson(doc, out);
  return out;
}

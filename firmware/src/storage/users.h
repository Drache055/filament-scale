#pragma once
#include <Arduino.h>

#include "config.h"

// Benutzerliste (NVS). Jeder Benutzer hat eine fortlaufende ID, die auch auf seinem NFC-Tag steht.
// Der Tag enthält zusätzlich ein geheimes Token (NVS-Schlüssel "tok<id>"), damit die ID allein nicht anmeldet.
struct User {
  uint16_t id = 0;
  char name[USER_NAME_LEN] = "";
  uint8_t lang = 0;  // eigene Sprache: 0 = Gerätesprache, 1 = Deutsch, 2 = Englisch
  bool canTare = false;  // Recht, die Waage am Gerät zu tarieren (vergibt der Admin)
  bool canCatalog = false;  // Recht, Filament-Typen und Hersteller zu pflegen (vergibt der Admin)
  bool weighOnly = false;   // Rolle "Nur wiegen": am Gerät nur der einfache Wiegeablauf, kein Spoolman (vergibt der Admin)
};

void usersBegin();
int usersList(User *out, int cap);          // Anzahl der Benutzer
bool usersFind(uint16_t id, User &out);
bool usersAdd(const char *name, String &error, uint16_t *newId = nullptr);
bool usersRemove(uint16_t id);
bool usersSetTare(uint16_t id, bool allowed);
bool usersSetCatalog(uint16_t id, bool allowed);
bool usersSetWeighOnly(uint16_t id, bool on);
bool usersSetLang(uint16_t id, uint8_t lang);  // 0 = Gerätesprache, 1 = Deutsch, 2 = Englisch
// Web-PIN (4 bis 6 Ziffern), nur als Salted-SHA-256-Hash im NVS. ID 0 = Generic-Zugang (nur wiegen und NFC lesen).
bool usersSetPin(uint16_t id, const char *pin, String &error);  // leerer Text entfernt die PIN
bool usersHasPin(uint16_t id);
bool usersCheckPin(uint16_t id, const char *pin);
// Benutzer-Tag: speichert das Token des zuletzt geschriebenen Tags (ein älterer Tag des Benutzers wird ungültig).
bool usersSetTagToken(uint16_t id, const String &token);
bool usersHasTagToken(uint16_t id);  // false: noch nie ein Tag mit Token geschrieben (alter Tag muss neu geschrieben werden)
String usersTagToken(uint16_t id);   // gespeichertes Token, leer = keins
bool usersFindByName(const char *name, User &out);  // Groß-/Kleinschreibung egal
bool usersGenericEnabled();
void usersSetGenericEnabled(bool on);

String usersJson();                         // [{"id":1,"name":"Fabian","lang":"de","tare":false,"catalog":false,"weigh_only":false,"pin":false,"tag":true}], lang "" = Gerätesprache, tag false = Tag neu schreiben

#pragma once
#include <Arduino.h>

// Web-Anmeldungen (Sitzungen im RAM, Cookie "sid"). Rollen: Generic (nur wiegen/NFC lesen), User (mit PIN), Admin.
// Eine Sitzung endet nach WEB_SESSION_MS ohne Anfrage oder beim Neustart des Geräts.
enum class WebRole : uint8_t { None = 0, Generic = 1, User = 2, Admin = 3 };

struct WebSession {
  WebRole role = WebRole::None;
  uint16_t userId = 0;  // nur bei User
};

String webSessionCreate(WebRole role, uint16_t userId);          // gibt das Token zurück (32 Hex-Zeichen)
bool webSessionGet(const String &token, WebSession &out);        // gültig? verlängert die Sitzung
void webSessionDestroy(const String &token);
void webSessionsDropUser(uint16_t userId);                       // Benutzer gelöscht oder PIN geändert
void webSessionsDropAdminExcept(const String &keepToken);        // Admin-Passwort geändert

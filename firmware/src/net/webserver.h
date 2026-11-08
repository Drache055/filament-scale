#pragma once

// Weboberfläche über HTTP (Port 80). Lesen ist offen, schreibende Aktionen verlangen Basic-Auth
// (Benutzer "admin", Passwort = OTA_PASSWORD aus secrets.h).
// Muss regelmäßig aus dem net-Task aufgerufen werden; startet erst, wenn WLAN verbunden ist.
void webLoop();

#pragma once

// Weboberfläche über HTTP (Port 80). Anmeldung und Rollen siehe CLAUDE.md 11.4.
// Muss regelmäßig aus dem net-Task aufgerufen werden; startet, sobald WLAN oder Hotspot läuft.
void webLoop();

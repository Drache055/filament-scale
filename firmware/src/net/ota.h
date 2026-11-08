#pragma once

// ArduinoOTA starten, sobald WLAN verbunden ist, und im Betrieb bedienen.
// Muss regelmäßig aus dem net-Task aufgerufen werden.
void otaLoop();

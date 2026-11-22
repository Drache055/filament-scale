#pragma once
#include <Arduino.h>

// Einrichtung per USB (CLAUDE.md 12.1): nimmt Pakete der Einrichtungsmaske auf der Webseite an (Protokoll in
// core/usb_setup_core.h). Alle anderen Bytes gehen an den Zeilenpuffer der seriellen Textbefehle.
// Aus loop() mit jedem empfangenen Byte aufrufen; gibt die Bytes zurück, die normaler Text sind.
void usbSetupFeed(uint8_t b, String &text);

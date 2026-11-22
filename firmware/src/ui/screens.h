#pragma once
#include <lvgl.h>

#include "net/spoolman.h"
#include "nfc/nfc.h"

// Gemeinsame Helfer und Navigation. Nur im ui-Task verwenden.
lv_obj_t *uiMakeButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb, bool fullWidth = false,
                       void *userData = nullptr);
lv_obj_t *uiNewScreen();  // Screen mit Theme-Style, Spaltenlayout
void uiNavigate(lv_obj_t *scr);  // Wechselt den Screen, räumt Unterscreens auf (Home bleibt bestehen)
void uiShowHome();

void screenMenuOpen();

// Admin-Sperre: true = erlaubt (Sperre aus oder Admin angemeldet). Sonst öffnet sich die Anmeldung (Passwort per
// Tastatur oder Admin-Tag), und onSuccess wird nach erfolgreicher Anmeldung aufgerufen.
bool uiAdminGate(void (*onSuccess)());
void screenAdminLoginOpen(void (*onSuccess)());
void screenMyLanguageOpen();  // eigene Sprache des angemeldeten Benutzers
void screenSettingsOpen();
void screenWifiOpen();
void screenDisplayOpen();
void screenLanguageOpen();
void uiRetranslateHome();  // statische Texte des Home-Screens neu setzen
void screenCalibrationOpen();
void uiOnTare();  // Tara aus dem Home-Screen
void screenNfcOpen();
void screenBoxWriteOpen();
// Statustext zu einem Schreibergebnis (Schreiben, Ok, Fehler). nullptr bei Idle/WaitingForTag. color = Textfarbe.
const char *uiNfcWriteText(NfcWrite st, lv_color_t *color);
void screenFilamentTypeOpen();
void screenFilamentMakerOpen();
void screenFilamentWriteOpen();
void screenTouchCalOpen();
void uiLanguageHandled();  // nach einer Sprachänderung am Gerät aufrufen, damit die UI sie nicht noch einmal als externe Änderung behandelt
void screenFilamentColorOpen();
void screenAssignSpoolOpen();  // nimmt den aktuell aufliegenden Tag (UID + Inhalt)

// Wiegeablauf (Differenzwägung): Spule erkannt -> Box [-> Neue Box anlegen] -> Spule in die Box -> Bestätigen -> Spoolman.
// Nur im ui-Task. spoolUid = UID des Spulen-Tags, der den Ablauf gestartet hat (wird beim Box-Schritt nie als neue Box
// angeboten, auch wenn er kein bekanntes Format hat).
bool weighFlowActive();
void weighFlowStart(const SpoolInfo &spool, const char *spoolUid, const char *owner);  // owner leer = nicht ändern
void weighFlowAbort();  // bricht einen laufenden Ablauf ohne Navigation ab (Screen wird von außen verlassen)

#pragma once
#include <Arduino.h>

#include "i18n/i18n.h"
#include "storage/users.h"

// Angemeldeter Benutzer ("Owner"). Ein Benutzer-Tag an der Waage meldet an, ein anderer Benutzer-Tag wechselt,
// nach USER_TIMEOUT_MS ohne Aktivität (Tag-Scans) wird abgemeldet. Wird später für das Spoolman-Feld "owner" genutzt.
void sessionBegin();
bool sessionCurrentUser(User &out);
bool sessionUserMayTare();  // angemeldeter Benutzer mit Tara-Recht (vom Admin vergeben)
bool sessionUserWeighOnly();  // angemeldeter Benutzer mit der Rolle "Nur wiegen"
// Einfacher Wiegeablauf erlaubt: Betriebsart "Einfache Waage" oder angemeldeter Benutzer mit "Nur wiegen"
bool sessionSimpleWeighAllowed();
void sessionLogout();  // meldet den Benutzer ab und stellt die Gerätesprache wieder her

// Eigene Sprache des angemeldeten Benutzers speichern und anwenden. false, wenn niemand angemeldet ist.
bool sessionSetMyLanguage(Lang lang);

// Daten eines Benutzers wurden geändert (z. B. Sprache über die Webseite): ist er angemeldet, wird neu geladen.
void sessionUserChanged(uint16_t id);

// Kurze Meldung für die Anzeige (z. B. "Angemeldet: Fabian"). level: 0 = Info, 1 = Warnung/Fehler.
bool sessionNotice(char *text, size_t cap, int *level);

// Meldung von außen (z. B. Wiegeablauf: Zeitüberschreitung) in der Hinweiszeile von Home anzeigen. level: 0 = Info, 1 = Fehler.
void sessionPostNotice(const char *text, int level);

#include <Arduino.h>
#include "util/log.h"
#include <Preferences.h>

#include "i18n.h"

namespace {

// Reihenfolge muss der Enum Str entsprechen
const char *const texts[(int)Str::Count][2] = {
    /* Menu */ {"Menü", "Menu"},
    /* Settings */ {"Einstellungen", "Settings"},
    /* WiFi */ {"WiFi", "WiFi"},
    /* Display */ {"Display", "Display"},
    /* Language */ {"Sprache", "Language"},
    /* Back */ {"Zurück", "Back"},
    /* Tare */ {"Tara", "Tare"},
    /* ScanSpool */ {"Spule scannen", "Scan spool"},
    /* WifiConnectedShort */ {"WLAN: verbunden", "WiFi: connected"},
    /* WifiDisconnectedShort */ {"WLAN: getrennt", "WiFi: disconnected"},
    /* Connected */ {"Verbunden", "Connected"},
    /* NotConnected */ {"Nicht verbunden", "Not connected"},
    /* Ssid */ {"SSID", "SSID"},
    /* Ip */ {"IP", "IP"},
    /* Signal */ {"Signal", "Signal"},
    /* Quality */ {"Qualität", "Quality"},
    /* QualityGood */ {"gut", "good"},
    /* QualityMedium */ {"mittel", "medium"},
    /* QualityWeak */ {"schwach", "weak"},
    /* VcomsTitle */ {"VCOMS (Schatten)", "VCOMS (ghosting)"},
    /* Save */ {"Speichern", "Save"},
    /* Default */ {"Standard", "Default"},
    /* Saved */ {"Gespeichert", "Saved"},
    /* NotSaved */ {"Nicht gespeichert", "Not saved"},
    /* German */ {"Deutsch", "Deutsch"},
    /* English */ {"English", "English"},
    /* Calibration */ {"Kalibrierung", "Calibration"},
    /* CalEmptyHint */ {"Waage leeren, dann Tara drücken.", "Empty the scale, then press tare."},
    /* TareContinue */ {"Tara & weiter", "Tare & continue"},
    /* PlaceWeight */ {"Gewicht auflegen", "Place weight"},
    /* Reference */ {"Referenz", "Reference"},
    /* Calibrate */ {"Kalibrieren", "Calibrate"},
    /* CalOk */ {"Kalibriert", "Calibrated"},
    /* CalFailed */ {"Fehler: Signal zu klein", "Failed: signal too small"},
    /* NotCalibrated */ {"Nicht kalibriert", "Not calibrated"},
    /* NoSensor */ {"Wägesensor fehlt", "No load sensor"},
    /* Stable */ {"stabil", "stable"},
    /* Overload */ {"Überlast", "Overload"},
    /* Raw */ {"Rohwert", "Raw"},
    /* Nfc */ {"NFC", "NFC"},
    /* NfcNoModule */ {"PN532 nicht gefunden", "PN532 not found"},
    /* NfcReady */ {"Bereit, Tag auflegen", "Ready, place a tag"},
    /* NfcTagUid */ {"UID", "UID"},
    /* NfcTagType */ {"Typ", "Type"},
    /* NfcCapacity */ {"Kapazität", "Capacity"},
    /* NfcLocked */ {"gesperrt", "locked"},
    /* KindSpool */ {"Spule", "Spool"},
    /* KindBox */ {"Box", "Box"},
    /* KindUnknown */ {"leer/unbekannt", "empty/unknown"},
    /* WriteBoxTag */ {"Box-Tag schreiben", "Write box tag"},
    /* BoxNumber */ {"Box-Nummer", "Box number"},
    /* Write */ {"Schreiben", "Write"},
    /* PlaceTag */ {"Tag auflegen ...", "Place tag ..."},
    /* Writing */ {"Schreibe ...", "Writing ..."},
    /* WriteOk */ {"Geschrieben", "Written"},
    /* WriteLocked */ {"Tag ist gesperrt", "Tag is locked"},
    /* WriteTooBig */ {"Passt nicht auf den Tag", "Does not fit on tag"},
    /* WriteNotNtag */ {"Kein NTAG erkannt", "Not an NTAG"},
    /* WriteFailed */ {"Schreiben fehlgeschlagen", "Write failed"},
    /* WriteTimeout */ {"Kein Tag erkannt", "No tag detected"},
    /* Filament */ {"Filament", "Filament"},
    /* FilamentType */ {"Filament-Typ", "Filament type"},
    /* Manufacturer */ {"Hersteller", "Manufacturer"},
    /* WriteFilamentTag */ {"Filament-Tag schreiben", "Write filament tag"},
    /* TouchCal */ {"Touch kalibrieren", "Calibrate touch"},
    /* TouchTap */ {"Fadenkreuz berühren", "Tap the crosshair"},
    /* TouchDone */ {"Gespeichert", "Saved"},
    /* TouchFail */ {"Fehlgeschlagen, nochmal", "Failed, try again"},
    /* User */ {"Benutzer", "User"},
    /* UserLoggedIn */ {"Angemeldet", "Logged in"},
    /* UserUnknown */ {"Unbekannter Benutzer", "Unknown user"},
    /* UserTagOutdated */ {"Tag veraltet, im Web neu schreiben", "Outdated tag, rewrite it on the web"},
    /* WriteOccupied */ {"Tag enthält eine Spule", "Tag holds a spool"},
    /* Spoolman */ {"Spoolman", "Spoolman"},
    /* SpoolmanUnknown */ {"Nicht in Spoolman", "Not in Spoolman"},
    /* SpoolmanReachable */ {"erreichbar", "reachable"},
    /* SpoolmanOffline */ {"nicht erreichbar", "unreachable"},
    /* Color */ {"Farbe", "Color"},
    /* QueuePending */ {"Ausstehende Updates: %d", "Pending updates: %d"},
    /* UnknownSpool */ {"Unbekannte Spule", "Unknown spool"},
    /* AssignSpool */ {"Spule zuordnen", "Assign spool"},
    /* NoSpools */ {"Keine Spulen in Spoolman", "No spools in Spoolman"},
    /* Loading */ {"Lade ...", "Loading ..."},
    /* Linking */ {"Verknüpfe ...", "Linking ..."},
    /* AssignDone */ {"Spule zugeordnet", "Spool assigned"},
    /* SetupWifi */ {"Einrichtungs-WLAN", "Setup WiFi"},
    /* HotspotHint */ {"WLAN einrichten: %s", "Set up WiFi: %s"},
    /* HotspotPassword */ {"Passwort: %s", "Password: %s"},
    /* HotspotPasswordWhere */ {"Passwort: Einstellungen > WiFi", "Password: Settings > WiFi"},
    /* DevicePassword */ {"Hotspot-Passwort: %s", "Hotspot password: %s"},
    /* AdminLogin */ {"Admin-Anmeldung", "Admin login"},
    /* Password */ {"PIN", "PIN"},
    /* WrongPassword */ {"Falsche PIN", "Wrong PIN"},
    /* AdminLoggedIn */ {"Admin angemeldet", "Admin logged in"},
    /* AdminTagUnknown */ {"Unbekannter Admin-Tag", "Unknown admin tag"},
    /* AdminOnly */ {"Nur mit Admin", "Admin only"},
    /* MyLanguage */ {"Meine Sprache", "My language"},
    /* KindAdmin */ {"Admin-Tag", "Admin tag"},
    /* AdminLogout */ {"Admin abmelden", "Admin log out"},
    /* AdminExpired */ {"Admin abgemeldet", "Admin logged out"},
    /* TooManyTries */ {"Zu viele Versuche, bitte warten", "Too many attempts, please wait"},
    /* WfSpoolFound */ {"Spule erkannt", "Spool detected"},
    /* WfEmptyScale */ {"Waage leeren", "Empty the scale"},
    /* WfPlaceBox */ {"Box auf die Waage stellen", "Place the box on the scale"},
    /* WfSearchBoxTag */ {"Box-Tag wird gesucht ...", "Looking for box tag ..."},
    /* WfPlaceSpool */ {"Spule in die Box stellen", "Put the spool in the box"},
    /* WfConfirm */ {"Bestätigen", "Confirm"},
    /* WfRemaining */ {"Restgewicht", "Remaining"},
    /* WfEmptySpool */ {"Leerspule", "Empty spool"},
    /* WfLastRemaining */ {"Letzter Rest: %d g", "Last remaining: %d g"},
    /* DryDue */ {"Trocknen fällig!", "Drying due!"},
    /* DryOk */ {"Getrocknet %s, noch %d Tage", "Dried %s, %d days left"},
    /* DryNone */ {"Trocknung nicht erfasst", "No drying logged"},
    /* DryLog */ {"Trocknung eintragen", "Log drying"},
    /* WfLength */ {"ca. %d m Filament", "approx. %d m of filament"},
    /* WfSaving */ {"Speichere ...", "Saving ..."},
    /* WfQueued */ {"In Warteschlange", "Queued"},
    /* WfRemoveBox */ {"Box entnehmen", "Remove the box"},
    /* WfCheckEmpty */ {"Leerspulengewicht prüfen", "Check empty spool weight"},
    /* WfClamped */ {"Unter 0 g, auf 0 gesetzt", "Below 0 g, set to 0"},
    /* WfTooHeavy */ {"Mehr als Anfangsgewicht, prüfen", "More than initial weight, check"},
    /* WfArchiveOff */ {"Archivieren: nein", "Archive: no"},
    /* WfArchiveOn */ {"Archivieren: ja", "Archive: yes"},
    /* WfTimeout */ {"Zeit abgelaufen", "Timed out"},
    /* WfCancel */ {"Abbrechen", "Cancel"},
    /* WfRetry */ {"Nochmal", "Retry"},
    /* WfDone */ {"Fertig", "Done"},
    /* WfSaveFailed */ {"Speichern fehlgeschlagen", "Saving failed"},
    /* WfNewBox */ {"Neue Box anlegen", "Set up new box"},
    /* WfNewBoxHint */ {"Der Tag der Box ist leer oder unbekannt.", "The box tag is empty or unknown."},
    /* WfQrHint */ {"QR-Code scannen", "Scan QR code"},
    /* SwWeigh */ {"Filament wiegen", "Weigh filament"},
    /* SwChoose */ {"Was kommt auf die Waage?", "What goes on the scale?"},
    /* SwSpoolOnly */ {"Nur Spule", "Spool only"},
    /* SwBoxSpool */ {"Box + Spule", "Box + spool"},
    /* SwPlaceBox */ {"Box auflegen", "Place the box"},
    /* SwBoxHint */ {"Wenn das Gewicht stabil ist: Box tarieren", "When the weight is stable: tare the box"},
    /* SwTareBox */ {"Box tarieren", "Tare box"},
    /* SwPlaceSpool */ {"Spule auflegen", "Place the spool"},
    /* SwGross */ {"Spule gesamt: %d g", "Spool total: %d g"},
    /* SwGrossBox */ {"Spule %d g • Box %d g", "Spool %d g • box %d g"},
    /* SwSpoolInBox */ {"Spule in die Box", "Spool into the box"},
    /* SwReady */ {"Einfache Waage", "Simple scale"},
};

Lang current = Lang::DE;
Lang deviceDefault = Lang::DE;  // gespeicherte Gerätesprache; current weicht ab, solange ein Benutzer mit eigener Sprache angemeldet ist
volatile uint32_t generation = 0;

}  // namespace

void i18nBegin() {
  Preferences prefs;
  prefs.begin("ui", false);
  uint8_t v = prefs.getUChar("lang", (uint8_t)Lang::DE);
  prefs.end();
  current = (v == (uint8_t)Lang::EN) ? Lang::EN : Lang::DE;
  deviceDefault = current;
  logPrintf("[UI] Sprache: %s\n", current == Lang::EN ? "EN" : "DE");
}

Lang i18nGet() { return current; }

Lang i18nDefault() { return deviceDefault; }

void i18nSet(Lang lang) {
  current = lang;
  deviceDefault = lang;
  generation++;
  logPrintf("[UI] Sprache: %s\n", lang == Lang::EN ? "EN" : "DE");
  Preferences prefs;
  prefs.begin("ui", false);
  prefs.putUChar("lang", (uint8_t)lang);
  prefs.end();
}

void i18nApply(Lang lang) {
  if (current == lang) {
    return;
  }
  current = lang;
  generation++;
  logPrintf("[UI] Sprache (Benutzer): %s\n", lang == Lang::EN ? "EN" : "DE");
}

void i18nRestoreDefault() { i18nApply(deviceDefault); }

uint32_t i18nGeneration() { return generation; }

const char *tr(Str id) { return texts[(int)id][(int)current]; }

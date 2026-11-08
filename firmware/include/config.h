#pragma once
// Defaults (Schwellen, Timeouts, Hardware-Konstanten)

#define TOUCH_I2C_ADDR 0x15
#define DRAW_BUF_LINES 40  // Zeilen pro LVGL-Teilpuffer (240 x 40 Pixel)
#define THEME_FILE     "/theme.json"

// Waage (CLAUDE.md 6)
#define SCALE_WINDOW        10      // Samples im Stabilitätsfenster (10 Hz = 1 s)
#define SCALE_STABLE_G      1.0f    // max - min im Fenster
#define SCALE_EMPTY_G       5.0f    // |Gewicht| darunter gilt als leer
#define SCALE_OVERLOAD_G    4500.0f
#define SCALE_NO_DATA_MS    2000    // so lange ohne HX711-Daten gilt der Sensor als fehlend
#define SCALE_MIN_CAL_DELTA 1000    // minimaler Rohwert-Unterschied für eine Kalibrierung

// Wiegeablauf (CLAUDE.md 5)
#define WEIGH_ADDED_G        50.0f   // Objekt hinzugefügt: stabil und >= Referenz + Schwelle
#define WEIGH_TIMEOUT_MS    120000  // Timeout je Wartezustand
#define WEIGH_SHOW_SPOOL_MS 2000    // Mindestdauer des Screens "Spule erkannt"
#define WEIGH_DEFAULT_EMPTY_G 250   // Startwert der manuellen Eingabe, wenn kein Leerspulengewicht bekannt ist

// NFC (PN532, I²C, Adresse 0x24)
#define NFC_POLL_MS         250     // ca. 4 Hz
#define NFC_MISSES_FOR_GONE 3       // so viele erfolglose Polls, bis ein Tag als entfernt gilt
#define NFC_I2C_ADDR        0x24
#define NFC_INIT_RETRY_MS   15000   // Neuversuch, solange das Modul nicht antwortet
#define NFC_WRITE_TIMEOUT_MS 20000  // so lange wird auf einen Tag zum Beschreiben gewartet

// Filament-Katalog (Typen mit Temperaturen, Hersteller), über die Weboberfläche pflegbar
#define CATALOG_MAX_ENTRIES   24
#define CATALOG_NAME_LEN      16   // inkl. Nullbyte (Typ passt in SpoolTag.type)
#define CATALOG_MAKER_LEN     32

// Benutzer (Besitzer von Filament), per Weboberfläche angelegt, per NFC-Tag angemeldet
#define USERS_MAX        16
#define USER_NAME_LEN    24            // inkl. Nullbyte
#define USER_TIMEOUT_MS  (15UL * 60UL * 1000UL)  // automatische Abmeldung ohne Aktivität

// Admin: Einstellungen am Gerät und auf der Webseite nur mit Admin-Passwort oder Admin-Tag (Sperre vom Admin abschaltbar)
#define ADMIN_DEFAULT_PASSWORD "Filament"
#define ADMIN_PASSWORD_MIN     4
#define ADMIN_PASSWORD_MAX     32
#define ADMIN_DEFAULT_PIN      "1234"  // Geräte-PIN (Ziffernblock am Bildschirm), vom Admin auf der Webseite änderbar
#define ADMIN_PIN_MIN          4
#define ADMIN_PIN_MAX          6
#define ADMIN_TIMEOUT_MS       (5UL * 60UL * 1000UL)  // Admin-Anmeldung am Gerät endet ohne Bedienung nach dieser Zeit

#define WEB_SESSION_MS       (30UL * 60UL * 1000UL)  // Webseite: Abmeldung ohne Anfrage nach dieser Zeit

// Spoolman (API-Doku der Instanz: <URL>/api/v1/docs)
#define SPOOLMAN_TIMEOUT_MS      5000
#define SPOOLMAN_CHECK_MS        30000   // Erreichbarkeitsprüfung im Hintergrund
#define CATALOG_SYNC_MS          (10UL * 60UL * 1000UL)  // Katalog (Typen, Hersteller) alle 10 min mit Spoolman abgleichen
#define SPOOLMAN_OWNER_FIELD_KEY "owner" // Extra-Feld an der Spule (Text), wird bei Bedarf angelegt

// Offline-Warteschlange (Spoolman-Updates)
#define QUEUE_FILE        "/queue.jsonl"
#define QUEUE_MAX_ENTRIES 100
#define QUEUE_RETRY_MS    15000   // so oft wird versucht, die Warteschlange abzuarbeiten

// WLAN-Einrichtung
#define WIFI_AP_SSID         "Filamentwaage-Setup"
#define WIFI_AP_FALLBACK_MS  60000   // so lange ohne Verbindung, dann startet das Einrichtungs-WLAN
#define WIFI_AP_STABLE_MS    60000   // so lange verbunden, dann wird das Einrichtungs-WLAN wieder abgeschaltet

#pragma once

// Schutz der Weboberfläche gegen DNS-Rebinding und CSRF (ohne Arduino, getestet).

// Name des Pflicht-Headers für alles außer GET. Ein eigener Header macht jede fremde Anfrage zu einem
// CORS-Preflight, den die Waage nicht beantwortet; Formulare und text/plain-POSTs fremder Seiten scheitern.
#define WEB_GUARD_HEADER "X-Requested-With"

// Host-Header erlaubt? Erlaubt sind leer (kein Browser), IPv4- und IPv6-Adressen, der Gerätename
// und "<Gerätename>.local", jeweils mit beliebigem Port und Groß-/Kleinschreibung, Punkt am Ende erlaubt.
// Fremde Namen (DNS-Rebinding, auch Router-Namen wie "<Gerätename>.fritz.box") sind gesperrt.
bool webHostAllowed(const char *host, const char *deviceName);

// Pflicht-Header bei POST gesetzt (nicht leer)?
bool webGuardHeaderOk(const char *value);

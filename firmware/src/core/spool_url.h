#pragma once
#include <stddef.h>

// Link zur Spule in der Spoolman-Weboberfläche: "<Spoolman-URL>/spool/show/<id>".
// Schrägstriche am Ende der Basis-URL werden entfernt. Rückgabe: Länge, 0 bei leerer Basis, id < 0 oder zu kleinem Puffer.
size_t spoolShowUrl(const char *baseUrl, int spoolId, char *out, size_t cap);

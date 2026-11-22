#pragma once
#include <Arduino.h>

#include "core/drying_core.h"
#include "net/spoolman.h"

// Trocknungs-Erinnerung: Einstellungen im NVS (Namespace "drying", Schlüssel "cfg" als JSON), Auswertung je Spule.
// Das Datum "zuletzt getrocknet" steht in Spoolman im Extra-Feld "last_dried" (siehe net/spoolman).
void dryingBegin();
DryingConfig dryingConfig();                       // Kopie, threadsicher
bool dryingSetConfig(const DryingConfig &cfg);     // speichert (NVS)

// Heutiges Datum (lokale Zeit) als Tageszahl bzw. "JJJJ-MM-TT"; false/-1, solange die Uhr nicht gestellt ist.
long dryingToday();
bool dryingTodayText(char *out11);

// Status einer Spule: Box (location gesetzt) verlängert das Intervall um den Box-Faktor.
DryingStatus dryingStatusFor(const SpoolInfo &spool);

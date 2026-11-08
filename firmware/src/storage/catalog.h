#pragma once
#include <Arduino.h>

#include "config.h"

// Auswahllisten für Filament-Tags: Typen (mit Temperaturbereich) und Hersteller.
// Liegt im NVS (überlebt "uploadfs"), Standardwerte sind einkompiliert.
struct CatalogMaterial {
  char name[CATALOG_NAME_LEN] = "";
  int minTemp = 0;
  int maxTemp = 0;
};

struct CatalogColor {
  char name[CATALOG_NAME_LEN] = "";
  char hex[8] = "";  // RRGGBB, Großbuchstaben, ohne #
};

struct Catalog {
  CatalogMaterial materials[CATALOG_MAX_ENTRIES];
  int materialCount = 0;
  char makers[CATALOG_MAX_ENTRIES][CATALOG_MAKER_LEN];
  int makerCount = 0;
  CatalogColor colors[CATALOG_MAX_ENTRIES];
  int colorCount = 0;
};

void catalogBegin();
void catalogGet(Catalog &out);   // threadsicherer Schnappschuss
String catalogJson();            
bool catalogSet(const char *json, String &error);  // prüft, speichert und übernimmt

// Ergänzt Hersteller und Typen, die es noch nicht gibt (Name ohne Beachtung der Groß-/Kleinschreibung). Vorhandene Einträge
// bleiben unverändert, es wird nichts gelöscht. Einträge, die nicht passen (Name zu lang, Liste voll), zählen als "skipped".
struct CatalogMergeResult {
  int addedMakers = 0;
  int addedMaterials = 0;
  int skipped = 0;
  bool saved = true;
  String error;
};
CatalogMergeResult catalogMerge(const String *makers, int makerCount, const CatalogMaterial *materials, int materialCount);

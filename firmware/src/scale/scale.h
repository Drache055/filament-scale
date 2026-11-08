#pragma once
#include <cstdint>

// HX711-Waage: Messung, Stabilitätserkennung, Tara, Kalibrierung (CLAUDE.md Abschnitt 6).
struct ScaleReading {
  bool valid = false;       // HX711 liefert Daten
  bool calibrated = false;  // Kalibrierfaktor vorhanden
  bool stable = false;
  bool empty = false;       // stabil und |Gewicht| <= SCALE_EMPTY_G
  bool overload = false;
  float grams = 0;          // Mittelwert des Fensters, nur sinnvoll wenn calibrated
  long rawMean = 0;         // Mittelwert der Rohwerte im Fenster
  long rawNet = 0;          // rawMean - Nulloffset
};

void scaleBegin();                     // startet den scale-Task, lädt Kalibrierung aus NVS
ScaleReading scaleGet();               // threadsicherer Schnappschuss
bool scaleTare();                      // aktuellen Mittelwert als Nullpunkt setzen und speichern
bool scaleCalibrate(float knownGrams); // Faktor aus aktuellem Mittelwert und bekanntem Gewicht berechnen und speichern

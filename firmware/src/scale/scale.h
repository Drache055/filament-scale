#pragma once
#include <cstdint>

#include "core/sensor_core.h"

// Waage (HX711, NAU7802 oder ADS1232, wählbar): Messung, Stabilitätserkennung, Tara, Kalibrierung (CLAUDE.md Abschnitt 6).
struct ScaleReading {
  bool valid = false;       // Sensor liefert Daten
  bool calibrated = false;  // Kalibrierfaktor vorhanden
  bool stable = false;
  bool empty = false;       // stabil und |Gewicht| <= SCALE_EMPTY_G
  bool overload = false;
  float grams = 0;          // getrimmter Mittelwert des Fensters, nur sinnvoll wenn calibrated
  long rawMean = 0;         // getrimmter Mittelwert der Rohwerte im Fenster (ohne SCALE_TRIM Extremwerte je Seite)
  long rawNet = 0;          // rawMean - Nulloffset
};

void scaleBegin();                     // startet den scale-Task, lädt Kalibrierung aus NVS
ScaleReading scaleGet();               // threadsicherer Schnappschuss
bool scaleTare();                      // aktuellen Mittelwert als Nullpunkt setzen und speichern
bool scaleCalibrate(float knownGrams); // Faktor aus aktuellem Mittelwert und bekanntem Gewicht berechnen und speichern
void scaleSetIdle(bool idle);          // UI meldet IDLE (Home, kein Wiegeablauf): nur dann Nullpunkt-Nachführung

// Sensorwahl (Webseite, Admin). Gespeichert wird sofort, wirksam wird sie erst nach dem Neustart.
SensorConfig scaleSensorActive();  // läuft gerade
SensorConfig scaleSensorStored();  // gespeichert
bool scaleSensorSave(const SensorConfig &cfg, const char *&error);

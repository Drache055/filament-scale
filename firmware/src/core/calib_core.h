#pragma once
#include <cstdint>

// Mehrpunkt-Kalibrierung der Waage (CLAUDE.md 6.2). Reine Logik ohne Hardwarezugriff, getestet in test/test_calib.
// Ein Punkt ist ein bekanntes Gewicht mit dem zugehörigen Rohwert relativ zum Nullpunkt (rawNet = Rohwert - Tara-Offset).
// Der Nullpunkt (0, 0) ist immer implizit dabei. Umrechnung:
//   - 0 Punkte: nur der Faktor (alte Einpunkt-Kalibrierung aus NVS ohne Punktliste), Gramm = rawNet / Faktor.
//   - 1 Punkt:  Gerade durch Null und den Punkt, wie die bisherige Kalibrierung.
//   - ab 2 Punkten: stückweise linear durch Null und alle Punkte, oberhalb des schwersten Punkts mit der Steigung des
//     letzten Abschnitts weiter, unterhalb von Null mit der Steigung des ersten Abschnitts.
// Der gespeicherte Faktor ist die Ausgleichsgerade durch Null (kleinste Quadrate). Er dient der Stabilitätsprüfung und
// bleibt für eine ältere Firmware nach einem Rückschritt gültig.
constexpr int CAL_MAX_POINTS = 5;

struct CalPoint {
  int32_t raw = 0;  // rawNet beim Auflegen des Gewichts
  float grams = 0;  // bekanntes Gewicht
};

struct CalModel {
  int count = 0;               // 0..CAL_MAX_POINTS, aufsteigend nach Gewicht sortiert
  CalPoint pt[CAL_MAX_POINTS];
  float factor = 1.0f;         // Rohwert pro Gramm (Ausgleichsgerade durch Null)
};

enum class CalError : uint8_t {
  None = 0,
  BadWeight,    // Gewicht <= 0
  SmallSignal,  // |raw| < minDelta
  Full,         // schon CAL_MAX_POINTS Punkte mit anderem Gewicht
  WrongSign,    // Vorzeichen des Rohwerts passt nicht zu den übrigen Punkten
  NotMonotonic, // schwereres Gewicht mit kleinerem Rohwert (oder umgekehrt): Gewicht falsch eingegeben oder verrutscht
};

// Gewichte, die weniger als so viel auseinander liegen, gelten als derselbe Punkt (der neue ersetzt den alten)
constexpr float CAL_SAME_POINT_G = 1.0f;

float calToGrams(const CalModel &m, float rawNet);

// Einpunkt-Kalibrierung: verwirft alle Punkte und setzt nur diesen
CalError calSetSingle(CalModel &m, int32_t raw, float grams, int32_t minDelta);
// Punkt hinzufügen oder (bei gleichem Gewicht) ersetzen. Bei einem Fehler bleibt m unverändert.
CalError calAddPoint(CalModel &m, int32_t raw, float grams, int32_t minDelta);
// Faktor aus den Punkten neu berechnen (Ausgleichsgerade durch Null). Ohne Punkte bleibt er.
void calUpdateFactor(CalModel &m);
// Prüft eine geladene Punktliste (Anzahl, Sortierung, Vorzeichen, Monotonie). Ungültig: Punkte verwerfen.
bool calValid(const CalModel &m);

// Linearität: Abweichung jedes Punkts von der Ausgleichsgeraden durch Null, in Gramm (gemessen nach Gerade - bekannt).
struct CalLinearity {
  int count = 0;
  float dev[CAL_MAX_POINTS] = {};
  float maxAbsDev = 0;     // größte |Abweichung| in g
  float maxAbsDevPct = 0;  // dieselbe bezogen auf das schwerste Gewicht, in %
};
CalLinearity calLinearity(const CalModel &m);

#pragma once
#include <cstdint>

// Messfenster und Nullpunkt der Waage (CLAUDE.md 6.2). Reine Logik ohne Hardwarezugriff, nativ getestet.

// Kennwerte des Fensters nach dem Trimmen: die trim kleinsten und trim größten Rohwerte fallen weg.
// Ein einzelner Ausreißer (Störimpuls, Lesefehler) verschiebt so weder den Mittelwert noch blockiert er "stabil".
struct WindowStats {
  long mean = 0;  // Mittelwert der verbleibenden Rohwerte
  long lo = 0;    // kleinster verbleibender Rohwert
  long hi = 0;    // größter verbleibender Rohwert
  int used = 0;   // so viele Rohwerte flossen ein
};

// n Rohwerte (Reihenfolge egal, n <= 32). Bei kleinem n wird weniger getrimmt, mindestens ein Wert bleibt.
WindowStats scaleWindowStats(const long *samples, int n, int trim);

struct ZeroConfig {
  uint32_t settleMs = 3000;      // nach Datenbeginn so lange kein automatisches Nullen (Einschwingen des Wandlers)
  float bootG = 5.0f;            // Autonullung nach dem Start: |Gewicht| bis hier
  uint32_t bootWindowMs = 180000;  // so lange nach dem ersten Datenbeginn wird die Autonullung versucht
  float trackG = 2.0f;           // Nachführung: |Gewicht| unter diesem Wert
  uint32_t trackHoldMs = 2000;   // so lange muss die Waage leer, stabil und im Leerlauf sein
};

struct ZeroInput {
  uint32_t now = 0;     // ms
  bool stable = false;  // Fenster voll und stabil
  float grams = 0;      // mit dem aktuellen Nullpunkt
  bool idle = false;    // Home-Screen, kein Wiegeablauf (IDLE)
};

enum class ZeroAction : uint8_t {
  None,
  BootZero,  // einmalig nach dem Start: Nullpunkt auf den aktuellen Mittelwert
  Track,     // Nullpunkt-Nachführung: Nullpunkt auf den aktuellen Mittelwert
};

// Entscheidet, wann der Nullpunkt automatisch nachgezogen wird. Der Aufrufer setzt dann den Offset auf den Mittelwert.
class ZeroTracker {
 public:
  explicit ZeroTracker(const ZeroConfig &cfg = ZeroConfig()) : cfg_(cfg) {}

  ZeroAction step(const ZeroInput &in);
  void dataLost() { haveData_ = false; }  // Sensor liefert nichts mehr: Einschwingzeit beginnt beim nächsten Wert neu
  void manualZero() { holding_ = false; }  // Tara von Hand: Haltezeit neu beginnen
  bool bootDone() const { return bootDone_; }

 private:
  ZeroConfig cfg_;
  bool haveData_ = false;
  bool everData_ = false;
  bool bootDone_ = false;
  bool holding_ = false;
  uint32_t dataSince_ = 0;
  uint32_t bootStart_ = 0;
  uint32_t holdSince_ = 0;
};

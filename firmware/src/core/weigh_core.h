#pragma once
#include <cstdint>

// Ablauf der Differenzwägung (CLAUDE.md 5 und 6.1). Reine Logik ohne Hardwarezugriff.
// Zustände: Spule erkannt -> Box auflegen -> Spule in die Box -> Bestätigen -> Speichern -> Box entnehmen.
enum class WeighState : uint8_t {
  Idle,
  SpoolIdentified,  // Spule erkannt, Waage muss leer und stabil sein
  WaitBox,          // Box auf die Waage stellen (Gewicht > Schwelle, stabil, Box-Tag gelesen)
  WaitSpool,        // Spule in die Box stellen (Gewicht >= W1 + Schwelle, stabil)
  Confirm,          // Ergebnis anzeigen
  Upload,           // an Spoolman senden
  Done,             // gespeichert, Box entnehmen
  Failed,           // Senden fehlgeschlagen (kein Netz-Fehler, sondern z. B. HTTP 4xx)
};

enum class WeighCheck : uint8_t {
  Ok,
  Clamped,      // -20 g <= Rest < 0: auf 0 gesetzt, Hinweis
  EmptyTooBig,  // Rest < -20 g: Fehler "Leerspulengewicht prüfen", nicht speicherbar
  TooHeavy,     // Rest > Anfangsgewicht + 50 g: Warnung
};

struct WeighResult {
  float gross = 0;      // W2 - W1
  float remaining = 0;  // nach Plausibilitätsprüfung (nie < 0)
  WeighCheck check = WeighCheck::Ok;
};

// emptySpool < 0 = unbekannt (dann muss der Aufrufer vorher einen Wert vorgeben). initialWeight < 0 = unbekannt.
WeighResult weighCompute(float w1, float w2, float emptySpool, float initialWeight);

struct WeighInput {
  uint32_t now = 0;       // ms
  float grams = 0;
  bool stable = false;
  bool empty = false;     // stabil und |Gewicht| <= Schwelle "leer"
  bool boxTag = false;    // Box-Tag liegt gelesen auf
  bool cancel = false;
  bool save = false;      // [Speichern] wurde getippt (nur in Confirm)
  bool uploadDone = false;   // nur in Upload
  bool uploadFailed = false;
};

struct WeighConfig {
  float addedG = 50.0f;           // "Objekt hinzugefügt": stabil und >= Referenz + addedG
  uint32_t timeoutMs = 120000;    // je Wartezustand
  uint32_t showSpoolMs = 2000;    // Mindestdauer des Screens "Spule erkannt"
};

enum class WeighEnd : uint8_t { None, Cancelled, Timeout, Finished };

class WeighFlow {
 public:
  explicit WeighFlow(const WeighConfig &cfg = WeighConfig()) : cfg_(cfg) {}

  void start(uint32_t now);               // Idle -> SpoolIdentified
  void step(const WeighInput &in);        // Zustandswechsel auswerten
  WeighState state() const { return state_; }
  WeighEnd end() const { return end_; }   // gesetzt, sobald der Ablauf wieder Idle ist
  float w1() const { return w1_; }
  float w2() const { return w2_; }
  uint32_t enteredAt() const { return entered_; }
  uint32_t remainingMs(uint32_t now) const;  // verbleibende Zeit im Wartezustand (0 = kein Timeout aktiv)

 private:
  void enter(WeighState s, uint32_t now) { state_ = s; entered_ = now; }
  void finish(WeighEnd e) { state_ = WeighState::Idle; end_ = e; }

  WeighConfig cfg_;
  WeighState state_ = WeighState::Idle;
  WeighEnd end_ = WeighEnd::None;
  uint32_t entered_ = 0;
  float w1_ = 0;
  float w2_ = 0;
};

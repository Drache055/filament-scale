#pragma once
#include <cstdint>

// Einfache Waage (CLAUDE.md 5.4): Wiegen ohne Spoolman, ohne Anmeldung und ohne NFC. Reine Logik ohne Hardwarezugriff.
// Nur Spule:    Waage leeren -> Spule auflegen (Anzeige relativ zum Nullpunkt beim Start)
// Box + Spule:  Waage leeren -> Box auflegen, [Box übernehmen] (tariert) -> Spule in die Box (Anzeige relativ zur Box)
enum class SimpleState : uint8_t {
  Idle,
  Clear,    // Waage muss leer und stabil sein, dann wird der Nullpunkt übernommen
  WaitBox,  // Box auflegen und mit [Box übernehmen] bestätigen
  Measure,  // Spule auflegen bzw. in die Box stellen, Live-Anzeige
};

enum class SimpleEnd : uint8_t { None, Cancelled, Timeout, Finished };

struct SimpleInput {
  uint32_t now = 0;  // ms
  float grams = 0;
  bool stable = false;
  bool empty = false;    // stabil und |Gewicht| <= Schwelle "leer"
  bool cancel = false;   // [Abbrechen] bzw. [Fertig]
  bool takeBox = false;  // [Box übernehmen] (nur in WaitBox)
};

struct SimpleConfig {
  float addedG = 50.0f;         // Box bzw. Spule gilt als aufgelegt: stabil und >= Referenz + addedG
  uint32_t timeoutMs = 120000;  // je Wartezustand (in Measure nur, solange noch nichts gewogen wurde)
};

// Gewicht des Filaments: Spule gesamt minus Leerspule, nie unter 0
float simpleFilament(float spoolGross, float emptySpool);

class SimpleFlow {
 public:
  explicit SimpleFlow(const SimpleConfig &cfg = SimpleConfig()) : cfg_(cfg) {}

  void start(uint32_t now, bool withBox);  // Idle -> Clear
  void step(const SimpleInput &in);
  SimpleState state() const { return state_; }
  SimpleEnd end() const { return end_; }
  bool withBox() const { return withBox_; }
  bool canTakeBox(const SimpleInput &in) const;  // [Box übernehmen] ist jetzt möglich
  float zero() const { return zero_; }           // Nullpunkt beim Start
  float boxWeight() const { return box_ - zero_; }
  float reference() const { return withBox_ ? box_ : zero_; }
  float spoolGross(float grams) const;  // Gewicht über der Referenz, nie unter 0
  bool measured() const { return measured_; }  // in Measure lag schon einmal eine Spule stabil auf
  uint32_t remainingMs(uint32_t now) const;     // 0 = kein Timeout aktiv

 private:
  void enter(SimpleState s, uint32_t now) { state_ = s; entered_ = now; }
  void finish(SimpleEnd e) { state_ = SimpleState::Idle; end_ = e; }
  bool timeoutActive() const;

  SimpleConfig cfg_;
  SimpleState state_ = SimpleState::Idle;
  SimpleEnd end_ = SimpleEnd::None;
  bool withBox_ = false;
  bool measured_ = false;
  uint32_t entered_ = 0;
  float zero_ = 0;
  float box_ = 0;
};

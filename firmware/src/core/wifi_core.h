#pragma once
#include <cstdint>
#include <string>
#include <vector>

// WLAN-Aufträge (CLAUDE.md 8.1): Scan, USB-Einrichtung und Wiederverbindung laufen nur im net-Task. Webseite und
// USB-Einrichtung stellen Anfragen, der net-Task führt sie in wifiLoop() aus. Reine Logik ohne Hardwarezugriff, nativ getestet.

struct WifiNet {
  std::string ssid;
  int rssi = 0;
  bool open = false;
};

// Rohliste aus dem Scan zusammenfassen: versteckte Netze weg, je Name nur das stärkste Signal, höchstens maxCount Einträge.
// Die Reihenfolge der ersten Fundstelle bleibt erhalten.
std::vector<WifiNet> wifiMergeNetworks(const std::vector<WifiNet> &raw, size_t maxCount = 30);

enum class ScanState : uint8_t { Idle, Pending, Running, Done, Failed };

// Asynchroner Scan. Ein Ergebnis bleibt maxAgeMs gültig, ein Fehler failHoldMs (so lange sehen ihn alle Abfragen).
// Danach startet die nächste Anfrage einen neuen Scan. Mehrere Anfragen teilen sich einen laufenden Scan.
class ScanJob {
 public:
  explicit ScanJob(uint32_t maxAgeMs = 10000, uint32_t failHoldMs = 3000, uint32_t timeoutMs = 15000)
      : maxAgeMs_(maxAgeMs), failHoldMs_(failHoldMs), timeoutMs_(timeoutMs) {}

  // Anfrage (Webseite, USB). Gibt den Zustand danach zurück: Pending/Running = noch warten, Done/Failed = Ergebnis.
  ScanState request(uint32_t now);
  // net-Task
  bool wantsStart() const { return state_ == ScanState::Pending; }
  void started(uint32_t now);
  bool timedOut(uint32_t now) const { return state_ == ScanState::Running && now - startedAt_ >= timeoutMs_; }
  void finished(uint32_t now, std::vector<WifiNet> nets);
  void failed(uint32_t now);

  ScanState state() const { return state_; }
  const std::vector<WifiNet> &networks() const { return nets_; }

 private:
  uint32_t maxAgeMs_, failHoldMs_, timeoutMs_;
  ScanState state_ = ScanState::Idle;
  uint32_t startedAt_ = 0;
  uint32_t doneAt_ = 0;
  std::vector<WifiNet> nets_;
};

enum class ProvState : uint8_t { Idle, Pending, Connecting, Connected, Failed };

// Verbindungsversuch der USB-Einrichtung. Der Aufrufer (loop-Task) stellt die Anfrage und wartet auf Connected/Failed,
// der net-Task startet die Verbindung (wantsStart/started) und meldet in step() den Verbindungsstatus.
class ProvisionJob {
 public:
  // false, wenn schon ein Versuch läuft
  bool request(uint32_t timeoutMs);
  bool busy() const { return state_ == ProvState::Pending || state_ == ProvState::Connecting; }
  bool wantsStart() const { return state_ == ProvState::Pending; }
  void started(uint32_t now);
  // Nur in Connecting: verbunden -> Connected, Zeit abgelaufen -> Failed. Gibt den Zustand danach zurück.
  ProvState step(uint32_t now, bool connected);
  void fail() { state_ = ProvState::Failed; }  // z. B. Speichern im NVS gescheitert

  ProvState state() const { return state_; }

 private:
  ProvState state_ = ProvState::Idle;
  uint32_t timeoutMs_ = 0;
  uint32_t startedAt_ = 0;
};

// Grund der letzten Trennung (ESP-IDF wifi_err_reason_t) grob eingeordnet, für die Meldung der USB-Einrichtung.
// NotFound: WLAN nicht gefunden oder zu schwach. Auth: gefunden, Anmeldung gescheitert (meist falsches Passwort, selten
// schwaches Signal). Other: alles andere. None: kein Grund bekannt.
enum class WifiFail : uint8_t { None, NotFound, Auth, Other };
WifiFail wifiClassifyReason(uint8_t reason);
const char *wifiFailName(WifiFail f);  // "notfound", "auth", "other", "" (Protokoll der USB-Einrichtung)

// Wiederverbindung mit gespeicherten Zugangsdaten (CLAUDE.md 12.1). Die Arduino-Automatik gibt bei manchen
// Trennungsgründen auf (z. B. AUTH_FAIL), deshalb versucht es der net-Task selbst, ohne Ende: Versuch (attemptMs), dann
// Pause mit wachsender Länge (2 s, 5 s, 10 s, 20 s, 30 s, danach immer 60 s). Hängt jemand am Hotspot, mindestens
// clientPauseMs Pause, weil jeder Versuch alle Kanäle absucht und den Hotspot kurz stört.
class ReconnectPolicy {
 public:
  explicit ReconnectPolicy(uint32_t attemptMs = 15000, uint32_t clientPauseMs = 120000)
      : attemptMs_(attemptMs), clientPauseMs_(clientPauseMs) {}

  // Regelmäßig aufrufen. true = jetzt einen Versuch starten (danach started()).
  bool due(uint32_t now, bool connected, bool apHasClients);
  void started(uint32_t now);
  void failed(uint32_t now);  // Versuch gescheitert (auch von außen, z. B. USB-Einrichtung), die Pause beginnt jetzt
  uint32_t failures() const { return failures_; }
  uint32_t pauseMs(bool apHasClients) const;

 private:
  uint32_t attemptMs_, clientPauseMs_;
  bool attempting_ = false;
  bool wasConnected_ = false;
  uint32_t startedAt_ = 0;
  uint32_t pauseFrom_ = 0;
  uint32_t failures_ = 0;
};

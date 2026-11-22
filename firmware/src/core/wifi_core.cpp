#include "wifi_core.h"

#include <utility>

std::vector<WifiNet> wifiMergeNetworks(const std::vector<WifiNet> &raw, size_t maxCount) {
  std::vector<WifiNet> out;
  for (const WifiNet &n : raw) {
    if (n.ssid.empty()) {
      continue;  // verstecktes Netz
    }
    bool dup = false;
    for (WifiNet &o : out) {
      if (o.ssid == n.ssid) {
        if (n.rssi > o.rssi) {
          o.rssi = n.rssi;  // stärkstes Signal je Name
          o.open = n.open;
        }
        dup = true;
        break;
      }
    }
    if (!dup && out.size() < maxCount) {
      out.push_back(n);
    }
  }
  return out;
}

ScanState ScanJob::request(uint32_t now) {
  switch (state_) {
    case ScanState::Pending:
    case ScanState::Running:
      break;  // laufender Scan wird geteilt
    case ScanState::Done:
      if (now - doneAt_ >= maxAgeMs_) {
        state_ = ScanState::Pending;
      }
      break;
    case ScanState::Failed:
      if (now - doneAt_ >= failHoldMs_) {
        state_ = ScanState::Pending;
      }
      break;
    case ScanState::Idle:
      state_ = ScanState::Pending;
      break;
  }
  return state_;
}

void ScanJob::started(uint32_t now) {
  state_ = ScanState::Running;
  startedAt_ = now;
}

void ScanJob::finished(uint32_t now, std::vector<WifiNet> nets) {
  nets_ = std::move(nets);
  state_ = ScanState::Done;
  doneAt_ = now;
}

void ScanJob::failed(uint32_t now) {
  nets_.clear();
  state_ = ScanState::Failed;
  doneAt_ = now;
}

bool ProvisionJob::request(uint32_t timeoutMs) {
  if (busy()) {
    return false;
  }
  state_ = ProvState::Pending;
  timeoutMs_ = timeoutMs;
  return true;
}

void ProvisionJob::started(uint32_t now) {
  state_ = ProvState::Connecting;
  startedAt_ = now;
}

ProvState ProvisionJob::step(uint32_t now, bool connected) {
  if (state_ != ProvState::Connecting) {
    return state_;
  }
  if (connected) {
    state_ = ProvState::Connected;
  } else if (now - startedAt_ >= timeoutMs_) {
    state_ = ProvState::Failed;
  }
  return state_;
}

WifiFail wifiClassifyReason(uint8_t reason) {
  switch (reason) {
    case 0:
      return WifiFail::None;
    case 201:  // NO_AP_FOUND
    case 212:  // NO_AP_FOUND_IN_RSSI_THRESHOLD
      return WifiFail::NotFound;
    case 15:   // 4WAY_HANDSHAKE_TIMEOUT (typisch bei falschem WPA2-Passwort)
    case 202:  // AUTH_FAIL
    case 204:  // HANDSHAKE_TIMEOUT
      return WifiFail::Auth;
    default:
      return WifiFail::Other;
  }
}

const char *wifiFailName(WifiFail f) {
  switch (f) {
    case WifiFail::NotFound:
      return "notfound";
    case WifiFail::Auth:
      return "auth";
    case WifiFail::Other:
      return "other";
    default:
      return "";
  }
}

uint32_t ReconnectPolicy::pauseMs(bool apHasClients) const {
  static const uint32_t steps[] = {2000, 5000, 10000, 20000, 30000, 60000};
  const uint32_t last = sizeof(steps) / sizeof(steps[0]) - 1;
  uint32_t d = steps[failures_ < last ? failures_ : last];
  if (apHasClients && d < clientPauseMs_) {
    d = clientPauseMs_;
  }
  return d;
}

bool ReconnectPolicy::due(uint32_t now, bool connected, bool apHasClients) {
  if (connected) {
    attempting_ = false;
    failures_ = 0;
    wasConnected_ = true;
    return false;
  }
  if (wasConnected_) {
    wasConnected_ = false;  // Verbindung gerade verloren: kurze Pause, dann neu versuchen
    attempting_ = false;
    pauseFrom_ = now;
  }
  if (attempting_) {
    if (now - startedAt_ < attemptMs_) {
      return false;
    }
    failed(now);
  }
  return now - pauseFrom_ >= pauseMs(apHasClients);
}

void ReconnectPolicy::started(uint32_t now) {
  attempting_ = true;
  startedAt_ = now;
}

void ReconnectPolicy::failed(uint32_t now) {
  attempting_ = false;
  if (failures_ < 1000) {
    failures_++;
  }
  pauseFrom_ = now;
}

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

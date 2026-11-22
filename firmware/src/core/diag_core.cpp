#include "diag_core.h"

namespace {
void add(DiagItem *out, int &n, DiagComp c, DiagLevel l, const char *code) { out[n++] = {c, l, code}; }
}  // namespace

int diagRun(const DiagInput &in, DiagItem *out) {
  int n = 0;
  bool young = in.uptimeS < DIAG_STARTUP_S;

  // Wägesensor
  if (in.sensorValid) {
    if (in.overload) {
      add(out, n, DiagComp::Sensor, DiagLevel::Warn, "overload");
    } else if (!in.calibrated) {
      add(out, n, DiagComp::Sensor, DiagLevel::Warn, "not_calibrated");
    } else {
      add(out, n, DiagComp::Sensor, DiagLevel::Ok, "ok");
    }
  } else {
    add(out, n, DiagComp::Sensor, young ? DiagLevel::Info : DiagLevel::Error, young ? "starting" : "no_data");
  }

  // WLAN
  if (in.wifiConnected) {
    bool weak = in.wifiRssi != 0 && in.wifiRssi <= -80;
    add(out, n, DiagComp::Wifi, weak ? DiagLevel::Warn : DiagLevel::Ok, weak ? "weak" : "ok");
  } else if (!in.wifiHasCreds) {
    add(out, n, DiagComp::Wifi, DiagLevel::Warn, "no_creds");
  } else if (young) {
    add(out, n, DiagComp::Wifi, DiagLevel::Info, "starting");
  } else {
    add(out, n, DiagComp::Wifi, DiagLevel::Error, "disconnected");
  }

  if (in.simple) {
    return n;
  }

  // NFC
  if (in.nfcReady) {
    add(out, n, DiagComp::Nfc, DiagLevel::Ok, "ok");
  } else {
    add(out, n, DiagComp::Nfc, young ? DiagLevel::Info : DiagLevel::Error, young ? "starting" : "no_module");
  }

  // Spoolman
  if (in.smReachable) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Ok, "ok");
  } else if (!in.wifiConnected) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Warn, "no_wifi");
  } else if (!in.smChecked) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Info, "starting");
  } else if (in.smHttp == 404) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "wrong_url");
  } else if (in.smHttp == 401 || in.smHttp == 403) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "denied");
  } else if (in.smHttp >= 500) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "server_error");
  } else if (in.smHttp == 200) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "not_spoolman");
  } else if (in.smHttp > 0) {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "http_error");
  } else {
    add(out, n, DiagComp::Spoolman, DiagLevel::Error, "unreachable");
  }

  // Offline-Warteschlange
  if (in.queueDropped > 0) {
    add(out, n, DiagComp::Queue, DiagLevel::Warn, "dropped");
  } else if (in.queueCount > 0) {
    add(out, n, DiagComp::Queue, in.smReachable ? DiagLevel::Info : DiagLevel::Warn,
        in.smReachable ? "sending" : "waiting");
  } else {
    add(out, n, DiagComp::Queue, DiagLevel::Ok, "ok");
  }
  return n;
}

const char *diagCompName(DiagComp c) {
  switch (c) {
    case DiagComp::Sensor: return "sensor";
    case DiagComp::Nfc: return "nfc";
    case DiagComp::Wifi: return "wifi";
    case DiagComp::Spoolman: return "spoolman";
    case DiagComp::Queue: return "queue";
  }
  return "";
}

const char *diagLevelName(DiagLevel l) {
  switch (l) {
    case DiagLevel::Ok: return "ok";
    case DiagLevel::Info: return "info";
    case DiagLevel::Warn: return "warn";
    case DiagLevel::Error: return "error";
  }
  return "";
}

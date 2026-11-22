#include "usb_setup_core.h"

#include <cstring>

namespace usbsetup {

namespace {
const uint8_t kHeader[6] = {'I', 'M', 'P', 'R', 'O', 'V'};

std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
  return s.substr(a, b - a);
}
}  // namespace

Parser::Result Parser::feed(uint8_t b, Packet &out, std::string &passthrough) {
  buf_.push_back(b);
  size_t n = buf_.size();
  if (n <= 6) {
    if (b == kHeader[n - 1]) {
      return None;
    }
    // Kein Paketanfang: die gesammelten Bytes sind normaler Text. Das letzte Byte kann ein neuer Anfang sein.
    buf_.pop_back();
    passthrough.append(buf_.begin(), buf_.end());
    buf_.clear();
    if (b == kHeader[0]) {
      buf_.push_back(b);
    } else {
      passthrough.push_back((char)b);
    }
    return None;
  }
  if (n == 7 && b != kVersion) {
    passthrough.append(buf_.begin(), buf_.end());
    buf_.clear();
    return None;
  }
  if (n < 9) {
    return None;  // Typ, Länge
  }
  size_t len = buf_[8];
  if (n < 9 + len + 1) {
    return None;
  }
  uint8_t sum = 0;
  for (size_t i = 0; i + 1 < n; i++) sum += buf_[i];
  Result r = BadChecksum;
  if (sum == buf_[n - 1]) {
    out.type = buf_[7];
    out.data.assign(buf_.begin() + 9, buf_.begin() + 9 + len);
    r = Got;
  }
  buf_.clear();  // Inhalt eines (auch fehlerhaften) Pakets nie als Text weitergeben, er kann Passwörter enthalten
  return r;
}

bool parseRpc(const std::vector<uint8_t> &data, uint8_t &command, std::vector<std::string> &strings) {
  strings.clear();
  if (data.size() < 2 || data[1] != data.size() - 2) {
    return false;
  }
  command = data[0];
  size_t i = 2;
  while (i < data.size()) {
    size_t len = data[i++];
    if (i + len > data.size()) {
      strings.clear();
      return false;
    }
    strings.emplace_back(data.begin() + i, data.begin() + i + len);
    i += len;
  }
  return true;
}

std::vector<uint8_t> buildPacket(uint8_t type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> p(kHeader, kHeader + 6);
  p.push_back(kVersion);
  p.push_back(type);
  p.push_back((uint8_t)data.size());
  p.insert(p.end(), data.begin(), data.end());
  uint8_t sum = 0;
  for (uint8_t b : p) sum += b;
  p.push_back(sum);
  p.push_back('\n');  // wie ESPHome: Zeilenende, damit Terminals und Logs sauber bleiben
  return p;
}

std::vector<uint8_t> buildState(uint8_t state) { return buildPacket(TypeState, {state}); }
std::vector<uint8_t> buildError(uint8_t error) { return buildPacket(TypeError, {error}); }

static std::vector<uint8_t> rpcData(uint8_t command, const std::vector<std::string> &strings) {
  std::vector<uint8_t> d{command, 0};
  for (const std::string &s : strings) {
    size_t len = s.size() > 255 ? 255 : s.size();
    if (d.size() + 1 + len > 255) {
      break;  // Paket höchstens 255 Datenbytes
    }
    d.push_back((uint8_t)len);
    d.insert(d.end(), s.begin(), s.begin() + len);
  }
  d[1] = (uint8_t)(d.size() - 2);
  return d;
}

std::vector<uint8_t> buildResult(uint8_t command, const std::vector<std::string> &strings) {
  return buildPacket(TypeResult, rpcData(command, strings));
}

std::vector<uint8_t> buildRpc(uint8_t command, const std::vector<std::string> &strings) {
  return buildPacket(TypeRpc, rpcData(command, strings));
}

uint8_t validateSetup(const std::string &ssid, const std::string &wifiPw, const std::string &adminPw,
                      const char *defaultAdminPw, size_t adminMin, size_t adminMax) {
  std::string s = trim(ssid);
  if (s.empty() || s.size() > 32) {
    return ErrBadSsid;
  }
  if (!wifiPw.empty() && (wifiPw.size() < 8 || wifiPw.size() > 63)) {
    return ErrBadWifiPw;
  }
  if (adminPw.size() < adminMin || adminPw.size() > adminMax || adminPw == defaultAdminPw) {
    return ErrBadAdminPw;
  }
  for (char c : adminPw) {
    if ((uint8_t)c < 0x20 || c == ':') {
      return ErrBadAdminPw;
    }
  }
  return ErrNone;
}

}  // namespace usbsetup

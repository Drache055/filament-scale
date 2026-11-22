#include "update_core.h"

#include <ArduinoJson.h>
#include <cctype>
#include <cstring>

namespace {

// Liest eine Zahl ab p, setzt p dahinter. false, wenn keine Ziffer folgt.
bool readNum(const char *&p, int &out) {
  if (!isdigit((unsigned char)*p)) {
    return false;
  }
  long v = 0;
  while (isdigit((unsigned char)*p)) {
    v = v * 10 + (*p - '0');
    if (v > 100000) {
      return false;
    }
    p++;
  }
  out = (int)v;
  return true;
}

bool endsWith(const char *s, const char *suffix) {
  size_t a = strlen(s), b = strlen(suffix);
  return a >= b && strcmp(s + a - b, suffix) == 0;
}

void copy(char *dst, size_t size, const char *src) {
  if (!src) {
    src = "";
  }
  size_t n = strlen(src);
  if (n >= size) {
    n = size - 1;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

}  // namespace

FwVersion fwParseVersion(const char *s) {
  FwVersion v;
  if (!s) {
    return v;
  }
  const char *p = s;
  if (*p == 'v' || *p == 'V') {
    p++;
  }
  if (!readNum(p, v.major) || *p != '.') {
    return v;
  }
  p++;
  if (!readNum(p, v.minor)) {
    return v;
  }
  if (*p == '.') {
    p++;
    if (!readNum(p, v.patch)) {
      return v;
    }
  }
  if (*p && *p != '-') {
    return v;  // z. B. "v1.2x"
  }
  // Rest: [-zusatz][-N-g<hash>][-dirty], von hinten abbauen
  char rest[64];
  copy(rest, sizeof(rest), p);
  size_t n = strlen(rest);
  if (endsWith(rest, "-dirty")) {
    n -= 6;
    rest[n] = 0;
    v.ahead = 1;
  }
  // "-N-g<hex>" am Ende?
  char *g = strrchr(rest, '-');
  if (g && g[1] == 'g' && g[2]) {
    bool hex = true;
    for (const char *h = g + 2; *h; h++) {
      hex = hex && isxdigit((unsigned char)*h);
    }
    if (hex) {
      *g = 0;
      char *num = strrchr(rest, '-');
      int ahead = 0;
      const char *q = num ? num + 1 : nullptr;
      if (q && readNum(q, ahead) && *q == 0) {
        *num = 0;
        v.ahead = ahead > 0 ? ahead : 1;
      } else {
        *g = '-';  // doch kein git-describe-Anhang
      }
    }
  }
  v.prerelease = rest[0] == '-';
  if (v.prerelease) {
    const char *suf = rest + 1;
    v.rc = (suf[0] == 'r' || suf[0] == 'R') && (suf[1] == 'c' || suf[1] == 'C');
    const char *e = suf + strlen(suf);
    while (e > suf && isdigit((unsigned char)e[-1])) {
      e--;
    }
    const char *q = e;
    if (*q && !readNum(q, v.preNum)) {
      v.preNum = 0;
    }
  }
  v.valid = true;
  return v;
}

int fwCompareVersions(const FwVersion &a, const FwVersion &b) {
  if (a.valid != b.valid) {
    return a.valid ? 1 : -1;
  }
  if (!a.valid) {
    return 0;
  }
  if (a.major != b.major) {
    return a.major < b.major ? -1 : 1;
  }
  if (a.minor != b.minor) {
    return a.minor < b.minor ? -1 : 1;
  }
  if (a.patch != b.patch) {
    return a.patch < b.patch ? -1 : 1;
  }
  if (a.prerelease != b.prerelease) {
    return a.prerelease ? -1 : 1;  // v1.2-rc1 < v1.2
  }
  if (a.prerelease) {
    if (a.rc != b.rc) {
      return a.rc ? 1 : -1;  // v1.2-beta3 < v1.2-rc1
    }
    if (a.preNum != b.preNum) {
      return a.preNum < b.preNum ? -1 : 1;
    }
  }
  if (a.ahead != b.ahead) {
    return a.ahead < b.ahead ? -1 : 1;
  }
  return 0;
}

FwChannel fwReleaseChannel(const char *tag, bool ghPrerelease) {
  FwVersion v = fwParseVersion(tag);
  if (v.prerelease) {
    return v.rc ? FwChannel::Rc : FwChannel::Test;
  }
  return ghPrerelease ? FwChannel::Rc : FwChannel::Stable;  // ohne Zusatz, aber als Pre-Release markiert
}

bool fwUpdateAvailable(const char *latestTag, const char *currentVersion) {
  FwVersion latest = fwParseVersion(latestTag);
  if (!latest.valid) {
    return false;
  }
  FwVersion cur = fwParseVersion(currentVersion);
  return !cur.valid || fwCompareVersions(latest, cur) > 0;
}

bool fwIsDowngrade(const char *latestTag, const char *currentVersion) {
  FwVersion latest = fwParseVersion(latestTag);
  FwVersion cur = fwParseVersion(currentVersion);
  return latest.valid && cur.valid && fwCompareVersions(latest, cur) < 0;
}

const char *fwChannelName(FwChannel ch) {
  return ch == FwChannel::Test ? "test" : ch == FwChannel::Rc ? "rc" : "stable";
}

bool fwChannelFromName(const char *name, FwChannel &out) {
  if (!name) {
    return false;
  }
  if (!strcmp(name, "stable")) {
    out = FwChannel::Stable;
  } else if (!strcmp(name, "rc")) {
    out = FwChannel::Rc;
  } else if (!strcmp(name, "test")) {
    out = FwChannel::Test;
  } else {
    return false;
  }
  return true;
}

bool fwAllowedUrl(const char *url) {
  if (!url || strncmp(url, "https://", 8) != 0) {
    return false;
  }
  const char *host = url + 8;
  size_t len = strcspn(host, "/:?#");
  if (len == 0 || len > 100) {
    return false;
  }
  char h[101];
  memcpy(h, host, len);
  h[len] = 0;
  for (char *c = h; *c; c++) {
    *c = (char)tolower((unsigned char)*c);
  }
  if (strcmp(h, "github.com") == 0) {
    return true;
  }
  const char *dom = ".githubusercontent.com";
  return endsWith(h, dom) && strlen(h) > strlen(dom);
}

bool fwParseReleases(const char *json, size_t len, FwChannel channel, FwRelease &out, bool &found, const char *&error) {
  out = FwRelease();
  found = false;
  JsonDocument filter;
  JsonObject f = filter.add<JsonObject>();
  f["tag_name"] = true;
  f["draft"] = true;
  f["prerelease"] = true;
  f["html_url"] = true;
  f["body"] = true;
  f["assets"][0]["name"] = true;
  f["assets"][0]["size"] = true;
  f["assets"][0]["browser_download_url"] = true;
  f["assets"][0]["digest"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter)) || !doc.is<JsonArrayConst>()) {
    error = "Antwort von GitHub ist kein gültiges JSON";
    return false;
  }
  JsonObjectConst best;
  FwVersion bestVer;
  const char *bestUrl = nullptr;
  uint32_t bestSize = 0;
  char bestSha[65] = "";
  for (JsonObjectConst r : doc.as<JsonArrayConst>()) {
    const char *tag = r["tag_name"] | "";
    FwVersion v = fwParseVersion(tag);
    if ((r["draft"] | false) || !v.valid || strlen(tag) >= sizeof(out.tag)) {
      continue;
    }
    if ((int)fwReleaseChannel(tag, r["prerelease"] | false) > (int)channel) {
      continue;
    }
    const char *url = nullptr;
    uint32_t size = 0;
    char sha[65] = "";
    for (JsonObjectConst a : r["assets"].as<JsonArrayConst>()) {
      const char *name = a["name"] | "";
      const char *u = a["browser_download_url"] | "";
      if (endsWith(name, "-ota.bin") && fwAllowedUrl(u) && strlen(u) < sizeof(out.otaUrl)) {
        url = u;
        size = a["size"] | 0u;
        fwParseDigest(a["digest"] | "", sha);  // fehlt oder ungültig: sha bleibt leer, die Installation lehnt dann ab
        break;
      }
    }
    if (!url) {
      continue;
    }
    if (!found || fwCompareVersions(v, bestVer) > 0) {
      found = true;
      best = r;
      bestVer = v;
      bestUrl = url;
      bestSize = size;
      memcpy(bestSha, sha, sizeof(bestSha));
    }
  }
  if (!found) {
    return true;
  }
  const char *tag = best["tag_name"] | "";
  copy(out.tag, sizeof(out.tag), tag);
  out.channel = fwReleaseChannel(tag, best["prerelease"] | false);
  copy(out.otaUrl, sizeof(out.otaUrl), bestUrl);
  out.otaSize = bestSize;
  memcpy(out.otaSha256, bestSha, sizeof(out.otaSha256));
  copy(out.pageUrl, sizeof(out.pageUrl), best["html_url"] | "");
  const char *body = best["body"] | "";
  size_t n = strlen(body);
  if (n > FW_NOTES_LEN) {
    n = FW_NOTES_LEN;
    while (n > 0 && ((unsigned char)body[n] & 0xC0) == 0x80) {
      n--;  // nicht mitten in einem UTF-8-Zeichen abschneiden
    }
    out.notesCut = true;
  }
  memcpy(out.notes, body, n);
  out.notes[n] = 0;
  return true;
}

bool fwParseDigest(const char *digest, char out[65]) {
  out[0] = 0;
  if (!digest || strncmp(digest, "sha256:", 7) != 0) {
    return false;
  }
  const char *h = digest + 7;
  if (strlen(h) != 64) {
    return false;
  }
  for (int i = 0; i < 64; i++) {
    if (!isxdigit((unsigned char)h[i])) {
      return false;
    }
  }
  for (int i = 0; i < 64; i++) {
    out[i] = (char)tolower((unsigned char)h[i]);
  }
  out[64] = 0;
  return true;
}

void fwHashToHex(const uint8_t hash[32], char out[65]) {
  static const char *DIGITS = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out[2 * i] = DIGITS[hash[i] >> 4];
    out[2 * i + 1] = DIGITS[hash[i] & 15];
  }
  out[64] = 0;
}

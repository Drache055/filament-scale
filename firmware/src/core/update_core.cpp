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
    return a.prerelease ? -1 : 1;  // v1.2-beta < v1.2
  }
  if (a.ahead != b.ahead) {
    return a.ahead < b.ahead ? -1 : 1;
  }
  return 0;
}

bool fwUpdateAvailable(const char *latestTag, const char *currentVersion) {
  FwVersion latest = fwParseVersion(latestTag);
  if (!latest.valid || latest.prerelease) {
    return false;
  }
  FwVersion cur = fwParseVersion(currentVersion);
  return !cur.valid || fwCompareVersions(latest, cur) > 0;
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

bool fwParseRelease(const char *json, size_t len, FwRelease &out, const char *&error) {
  out = FwRelease();
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["draft"] = true;
  filter["prerelease"] = true;
  filter["html_url"] = true;
  filter["body"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["size"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) {
    error = "Antwort von GitHub ist kein gültiges JSON";
    return false;
  }
  const char *tag = doc["tag_name"] | "";
  if (!*tag) {
    error = "Release ohne Tag";
    return false;
  }
  if ((doc["draft"] | false) || (doc["prerelease"] | false) || fwParseVersion(tag).prerelease) {
    error = "Neueste Release ist keine stabile Version";
    return false;
  }
  if (strlen(tag) >= sizeof(out.tag)) {
    error = "Tag zu lang";
    return false;
  }
  copy(out.tag, sizeof(out.tag), tag);
  copy(out.pageUrl, sizeof(out.pageUrl), doc["html_url"] | "");
  for (JsonObjectConst a : doc["assets"].as<JsonArrayConst>()) {
    const char *name = a["name"] | "";
    const char *url = a["browser_download_url"] | "";
    if (endsWith(name, "-ota.bin") && fwAllowedUrl(url) && strlen(url) < sizeof(out.otaUrl)) {
      copy(out.otaUrl, sizeof(out.otaUrl), url);
      out.otaSize = a["size"] | 0u;
      break;
    }
  }
  if (!out.otaUrl[0]) {
    error = "Release enthält kein OTA-Image (*-ota.bin)";
    return false;
  }
  const char *body = doc["body"] | "";
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

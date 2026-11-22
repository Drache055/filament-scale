#include "web_guard.h"

#include <ctype.h>
#include <string.h>

// Eine Zahl 0-255 aus 1 bis 3 Ziffern, ohne führende Null (sonst deuten Browser sie oktal)
static bool octet(const char *s, size_t n) {
  if (n == 0 || n > 3 || (n > 1 && s[0] == '0')) {
    return false;
  }
  int v = 0;
  for (size_t i = 0; i < n; i++) {
    if (!isdigit((unsigned char)s[i])) {
      return false;
    }
    v = v * 10 + (s[i] - '0');
  }
  return v <= 255;
}

static bool isIpv4(const char *s, size_t n) {
  int parts = 0;
  size_t start = 0;
  for (size_t i = 0; i <= n; i++) {
    if (i == n || s[i] == '.') {
      if (!octet(s + start, i - start)) {
        return false;
      }
      parts++;
      start = i + 1;
    }
  }
  return parts == 4;
}

static bool equalsNoCase(const char *a, size_t n, const char *b) {
  if (strlen(b) != n) {
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
      return false;
    }
  }
  return true;
}

bool webHostAllowed(const char *host, const char *deviceName) {
  if (!host || !*host) {
    return true;  // Browser schicken immer einen Host
  }
  size_t n = strlen(host);
  if (host[0] == '[') {  // IPv6-Adresse, z. B. "[fe80::1]:80"
    const char *end = strchr(host, ']');
    if (!end || end == host + 1) {
      return false;
    }
    for (const char *p = host + 1; p < end; p++) {
      if (!isalnum((unsigned char)*p) && *p != ':' && *p != '.' && *p != '%') {  // inkl. Zonen-ID
        return false;
      }
    }
    return end[1] == '\0' || end[1] == ':';
  }
  const char *colon = strchr(host, ':');
  if (colon) {
    n = colon - host;
  }
  if (n > 0 && host[n - 1] == '.') {
    n--;
  }
  if (n == 0) {
    return false;
  }
  if (isIpv4(host, n)) {
    return true;
  }
  if (!deviceName || !*deviceName) {
    return false;
  }
  if (equalsNoCase(host, n, deviceName)) {
    return true;
  }
  size_t d = strlen(deviceName);
  return n == d + 6 && equalsNoCase(host, d, deviceName) && equalsNoCase(host + d, 6, ".local");
}

bool webGuardHeaderOk(const char *value) { return value && *value; }

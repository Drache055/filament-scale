#include "core/spool_url.h"

#include <stdio.h>
#include <string.h>

size_t spoolShowUrl(const char *baseUrl, int spoolId, char *out, size_t cap) {
  if (!out || cap == 0) return 0;
  out[0] = '\0';
  if (!baseUrl || spoolId < 0) return 0;
  size_t n = strlen(baseUrl);
  while (n > 0 && baseUrl[n - 1] == '/') n--;
  if (n == 0) return 0;
  int len = snprintf(out, cap, "%.*s/spool/show/%d", (int)n, baseUrl, spoolId);
  if (len <= 0 || (size_t)len >= cap) {
    out[0] = '\0';
    return 0;
  }
  return (size_t)len;
}

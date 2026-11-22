#include "uid_check.h"

#include <stddef.h>

bool isValidTagUid(const char *uid) {
  if (!uid) {
    return false;
  }
  size_t n = 0;
  for (; uid[n]; n++) {
    char c = uid[n];
    bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    if (!hex || n >= 20) {
      return false;
    }
  }
  return n >= 8 && n % 2 == 0;
}

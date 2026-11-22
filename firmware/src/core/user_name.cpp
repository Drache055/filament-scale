#include "core/user_name.h"

#include <ctype.h>
#include <string.h>

bool userNameReserved(const char *name) {
  if (!name) {
    return false;
  }
  while (isspace((unsigned char)*name)) {
    name++;
  }
  size_t len = strlen(name);
  while (len > 0 && isspace((unsigned char)name[len - 1])) {
    len--;
  }
  static const char *const reserved[] = {"admin", "generic"};
  for (const char *r : reserved) {
    if (strlen(r) == len) {
      size_t i = 0;
      while (i < len && tolower((unsigned char)name[i]) == r[i]) {
        i++;
      }
      if (i == len) {
        return true;
      }
    }
  }
  return false;
}

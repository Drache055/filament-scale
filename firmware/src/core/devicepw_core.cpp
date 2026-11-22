#include "devicepw_core.h"

#include <cstring>

namespace devicepw {

bool compiledUsable(const char *pw) {
  return pw && strlen(pw) >= kMinLength && strcmp(pw, kPlaceholder) != 0;
}

void generate(uint32_t (*rnd)(), char *out, size_t outSize) {
  if (!out || outSize == 0) {
    return;
  }
  const size_t n = sizeof(kAlphabet) - 1;
  size_t len = outSize - 1 < kLength ? outSize - 1 : kLength;
  for (size_t i = 0; i < len; i++) {
    // Rest-Verzerrung durch den Modulo ist bei 31 Zeichen und 32 Bit Zufall vernachlässigbar
    out[i] = kAlphabet[rnd() % n];
  }
  out[len] = '\0';
}

}  // namespace devicepw

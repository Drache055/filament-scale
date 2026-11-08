#include "secret.h"

#include <esp_random.h>
#include <mbedtls/sha256.h>

String secretRandomHex(size_t bytes) {
  String out;
  for (size_t i = 0; i < bytes; i++) {
    char b[3];
    snprintf(b, sizeof(b), "%02X", (unsigned)(esp_random() & 0xFF));
    out += b;
  }
  return out;
}

String secretHash(const String &salt, const char *secret) {
  String input = salt + (secret ? secret : "");
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char *)input.c_str(), input.length());
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  String out;
  for (int i = 0; i < 32; i++) {
    char b[3];
    snprintf(b, sizeof(b), "%02x", digest[i]);
    out += b;
  }
  return out;
}

bool secretEqual(const String &a, const String &b) {
  if (a.length() != b.length()) {
    return false;
  }
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); i++) {
    diff |= (uint8_t)a[i] ^ (uint8_t)b[i];
  }
  return diff == 0;
}

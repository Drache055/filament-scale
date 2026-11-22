#pragma once
// Minimaler Arduino-Ersatz, damit die UI-Quellen am Rechner übersetzt werden können (nur für tools/screenshots).
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

uint32_t millis();
// glibc ab 2.38 deklariert strlcpy selbst (mit throw()); eine eigene Deklaration kollidiert dann.
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 38))
#define STUB_LIBC_STRLCPY 1
#else
size_t strlcpy(char *dst, const char *src, size_t size);
#endif

template <class T> inline T max(T a, T b) { return a > b ? a : b; }
template <class T> inline T min(T a, T b) { return a < b ? a : b; }
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

class String {
 public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(int v) : s_(std::to_string(v)) {}
  const char *c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  String operator+(const char *o) const { return String(s_ + o); }
  String operator+(const String &o) const { return String(s_ + o.s_); }
  String &operator+=(const char *o) { s_ += o; return *this; }
  bool operator==(const char *o) const { return s_ == o; }
 private:
  std::string s_;
};

#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(uint32_t) {}
inline void vTaskDelete(void *) {}
inline void xTaskCreatePinnedToCore(void (*)(void *), const char *, int, void *, int, void *, int) {}

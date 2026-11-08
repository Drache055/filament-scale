#pragma once
#include <Arduino.h>
// Liest Dateien aus einem Verzeichnis auf dem Rechner (data/), damit theme.json wie am Gerät geladen wird.
class File {
 public:
  File() {}
  File(FILE *f) : f_(f) {}
  explicit operator bool() const { return f_ != nullptr; }
  void close() { if (f_) fclose(f_); f_ = nullptr; }
  int read() { return f_ ? fgetc(f_) : -1; }
  size_t readBytes(char *buf, size_t n) { return f_ ? fread(buf, 1, n, f_) : 0; }
 private:
  FILE *f_ = nullptr;
};
struct LittleFSClass {
  File open(const char *path, const char *mode = "r");
};
extern LittleFSClass LittleFS;

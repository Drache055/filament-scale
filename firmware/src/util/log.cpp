#include "log.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <stdarg.h>

namespace {

constexpr int LINES = 160;
constexpr int LINE_LEN = 128;

struct Line {
  uint32_t seq;
  uint32_t ms;
  char text[LINE_LEN];
};

Line *lines = nullptr;
uint32_t nextSeq = 1;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t previousVprintf = nullptr;

// Zeilen aus einem Textblock in den Ringpuffer schreiben (ANSI-Farbcodes entfernen)
void append(const char *text) {
  if (!lines) {
    return;
  }
  char clean[LINE_LEN];
  size_t n = 0;
  for (const char *p = text;; p++) {
    char c = *p;
    if (c == '\033') {  // ESC [ ... m
      while (*p && *p != 'm') {
        p++;
      }
      if (!*p) {
        break;
      }
      continue;
    }
    if (c == '\n' || c == '\0') {
      if (n > 0) {
        clean[n] = '\0';
        portENTER_CRITICAL(&lock);
        Line &l = lines[nextSeq % LINES];
        l.seq = nextSeq++;
        l.ms = millis();
        memcpy(l.text, clean, n + 1);
        portEXIT_CRITICAL(&lock);
        n = 0;
      }
      if (c == '\0') {
        break;
      }
      continue;
    }
    if (c == '\r') {
      continue;
    }
    if (n < LINE_LEN - 1) {
      clean[n++] = c;
    }
  }
}

int espLogHook(const char *fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  char buf[LINE_LEN * 2];
  vsnprintf(buf, sizeof(buf), fmt, copy);
  va_end(copy);
  append(buf);
  return previousVprintf ? previousVprintf(fmt, args) : vprintf(fmt, args);
}

}  // namespace

void logBegin() {
  lines = (Line *)heap_caps_calloc(LINES, sizeof(Line), MALLOC_CAP_SPIRAM);
  if (!lines) {
    lines = (Line *)calloc(LINES, sizeof(Line));
  }
  previousVprintf = esp_log_set_vprintf(espLogHook);
}

void logPrintf(const char *fmt, ...) {
  char buf[LINE_LEN * 2];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
  append(buf);
}

void logPrintln(const char *msg) {
  Serial.println(msg);
  append(msg);
}

uint32_t logRead(uint32_t sinceSeq, String &out, size_t maxBytes) {
  if (!lines) {
    return sinceSeq;
  }
  uint32_t last = sinceSeq;
  uint32_t newest;
  portENTER_CRITICAL(&lock);
  newest = nextSeq - 1;
  portEXIT_CRITICAL(&lock);
  uint32_t first = newest > LINES - 1 ? newest - (LINES - 1) : 1;
  if (sinceSeq + 1 > first) {
    first = sinceSeq + 1;
  }
  for (uint32_t s = first; s <= newest; s++) {
    Line copy;
    portENTER_CRITICAL(&lock);
    copy = lines[s % LINES];
    portEXIT_CRITICAL(&lock);
    if (copy.seq != s) {
      continue;  // inzwischen überschrieben
    }
    char head[24];
    snprintf(head, sizeof(head), "[%7.1f] ", copy.ms / 1000.0);
    if (out.length() + strlen(head) + strlen(copy.text) + 1 > maxBytes) {
      break;
    }
    out += head;
    out += copy.text;
    out += '\n';
    last = s;
  }
  return last;
}

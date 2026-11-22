#include "cbor.h"

#include <cmath>
#include <cstring>

namespace {

constexpr int MAX_DEPTH = 8;

double halfToDouble(uint16_t h) {
  int exp = (h >> 10) & 0x1F;
  int mant = h & 0x3FF;
  double v;
  if (exp == 0) {
    v = std::ldexp((double)mant, -24);
  } else if (exp != 31) {
    v = std::ldexp((double)(mant + 1024), exp - 25);
  } else {
    v = mant == 0 ? INFINITY : NAN;
  }
  return (h & 0x8000) ? -v : v;
}

uint64_t readBe(const uint8_t *p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) {
    v = (v << 8) | p[i];
  }
  return v;
}

}  // namespace

bool cborHead(const uint8_t *buf, size_t len, size_t pos, CborItem &out) {
  out = CborItem();
  if (pos >= len) {
    return false;
  }
  uint8_t ib = buf[pos];
  uint8_t major = ib >> 5;
  uint8_t ai = ib & 0x1F;
  size_t p = pos + 1;
  uint64_t val = 0;
  size_t extra = 0;
  if (ai < 24) {
    val = ai;
  } else if (ai >= 24 && ai <= 27) {
    extra = (size_t)1 << (ai - 24);
    if (p + extra > len) {
      return false;
    }
    val = readBe(buf + p, extra);
    p += extra;
  } else if (ai == 31) {
    if (major == 0 || major == 1 || major == 6) {
      return false;
    }
    if (major == 7) {
      out.type = CborType::Break;
      out.headerEnd = p;
      return true;
    }
    out.indefinite = true;
  } else {
    return false;  // 28 bis 30 reserviert
  }

  out.value = val;
  out.headerEnd = p;
  switch (major) {
    case 0: out.type = CborType::UInt; break;
    case 1: out.type = CborType::NInt; break;
    case 2:
    case 3:
      out.type = major == 2 ? CborType::Bytes : CborType::Text;
      if (!out.indefinite) {
        if (val > len - p) {
          return false;
        }
        out.data = buf + p;
        out.headerEnd = p + (size_t)val;
      }
      break;
    case 4: out.type = CborType::Array; break;
    case 5: out.type = CborType::Map; break;
    case 6: out.type = CborType::Tag; break;
    case 7:
      if (ai == 25) {
        out.type = CborType::Float;
        out.number = halfToDouble((uint16_t)val);
      } else if (ai == 26) {
        uint32_t u = (uint32_t)val;
        float f;
        memcpy(&f, &u, 4);
        out.type = CborType::Float;
        out.number = f;
      } else if (ai == 27) {
        double d;
        memcpy(&d, &val, 8);
        out.type = CborType::Float;
        out.number = d;
      } else {
        out.type = CborType::Simple;
      }
      break;
  }
  return true;
}

size_t cborSkip(const uint8_t *buf, size_t len, size_t pos, int depth) {
  if (depth > MAX_DEPTH) {
    return 0;
  }
  CborItem it;
  if (!cborHead(buf, len, pos, it) || it.type == CborType::Break) {
    return 0;
  }
  size_t p = it.headerEnd;
  switch (it.type) {
    case CborType::Bytes:
    case CborType::Text:
      if (it.indefinite) {  // Folge von Teilstrings bis Break
        for (;;) {
          CborItem chunk;
          if (!cborHead(buf, len, p, chunk)) {
            return 0;
          }
          if (chunk.type == CborType::Break) {
            return chunk.headerEnd;
          }
          if (chunk.type != it.type || chunk.indefinite) {
            return 0;
          }
          p = chunk.headerEnd;
        }
      }
      return p;
    case CborType::Array:
    case CborType::Map: {
      uint64_t items = it.type == CborType::Map ? it.value * 2 : it.value;
      if (it.indefinite) {
        for (;;) {
          CborItem next;
          if (!cborHead(buf, len, p, next)) {
            return 0;
          }
          if (next.type == CborType::Break) {
            return next.headerEnd;
          }
          p = cborSkip(buf, len, p, depth + 1);
          if (p == 0) {
            return 0;
          }
        }
      }
      if (items > len) {  // jedes Element braucht mindestens 1 Byte
        return 0;
      }
      for (uint64_t i = 0; i < items; i++) {
        p = cborSkip(buf, len, p, depth + 1);
        if (p == 0) {
          return 0;
        }
      }
      return p;
    }
    case CborType::Tag:
      return cborSkip(buf, len, p, depth + 1);
    default:
      return p;
  }
}

bool cborNumber(const CborItem &item, double &out) {
  switch (item.type) {
    case CborType::UInt: out = (double)item.value; return true;
    case CborType::NInt: out = -1.0 - (double)item.value; return true;
    case CborType::Float: out = item.number; return true;
    default: return false;
  }
}

size_t cborForEachInMap(const uint8_t *buf, size_t len, size_t pos, CborMapFn fn, void *ctx) {
  CborItem map;
  if (!cborHead(buf, len, pos, map) || map.type != CborType::Map) {
    return 0;
  }
  size_t p = map.headerEnd;
  for (uint64_t i = 0; map.indefinite || i < map.value; i++) {
    CborItem key;
    if (!cborHead(buf, len, p, key)) {
      return 0;
    }
    if (key.type == CborType::Break) {
      return map.indefinite ? key.headerEnd : 0;
    }
    size_t valuePos = cborSkip(buf, len, p);
    if (valuePos == 0) {
      return 0;
    }
    size_t next = cborSkip(buf, len, valuePos);
    if (next == 0) {
      return 0;
    }
    if (key.type == CborType::UInt && key.value <= INT64_MAX) {
      fn(ctx, (int64_t)key.value, buf, len, valuePos);
    } else if (key.type == CborType::NInt && key.value < INT64_MAX) {
      fn(ctx, -1 - (int64_t)key.value, buf, len, valuePos);
    }
    p = next;
  }
  return p;
}

void CborWriter::raw(uint8_t b) {
  if (pos >= cap) {
    ok = false;
    return;
  }
  buf[pos++] = b;
}

void CborWriter::head(uint8_t major, uint64_t value) {
  uint8_t m = (uint8_t)(major << 5);
  if (value < 24) {
    raw(m | (uint8_t)value);
    return;
  }
  int n;
  uint8_t ai;
  if (value <= 0xFF) {
    n = 1, ai = 24;
  } else if (value <= 0xFFFF) {
    n = 2, ai = 25;
  } else if (value <= 0xFFFFFFFFull) {
    n = 4, ai = 26;
  } else {
    n = 8, ai = 27;
  }
  raw(m | ai);
  for (int i = n - 1; i >= 0; i--) {
    raw((uint8_t)(value >> (8 * i)));
  }
}

void CborWriter::sint(int64_t v) {
  if (v >= 0) {
    head(0, (uint64_t)v);
  } else {
    head(1, (uint64_t)(-1 - v));
  }
}

void CborWriter::bytes(const uint8_t *data, size_t n) {
  head(2, n);
  for (size_t i = 0; i < n; i++) {
    raw(data[i]);
  }
}

void CborWriter::text(const char *s) {
  size_t n = strlen(s);
  head(3, n);
  for (size_t i = 0; i < n; i++) {
    raw((uint8_t)s[i]);
  }
}

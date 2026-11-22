#include "ndef.h"

#include <cstring>

bool ndefParseCc(const uint8_t cc[4], NdefCc &out) {
  if (cc[0] != 0xE1) {
    return false;
  }
  out.capacityBytes = (size_t)cc[2] * 8;
  out.readOnly = (cc[3] & 0x0F) != 0;  // unteres Nibble = Schreibzugriff, 0x0 = frei, 0xF = gesperrt
  return out.capacityBytes > 0;
}

NdefLocate ndefLocate(const uint8_t *data, size_t avail, NdefLocation &loc) {
  size_t i = 0;
  while (i < avail) {
    uint8_t tag = data[i];
    if (tag == 0x00) {  // NULL-TLV
      i++;
      continue;
    }
    if (tag == 0xFE) {  // Terminator
      return NdefLocate::None;
    }
    if (i + 1 >= avail) {
      loc.neededBytes = i + 4;
      return NdefLocate::NeedMore;
    }
    size_t len, header;
    if (data[i + 1] == 0xFF) {
      if (i + 3 >= avail) {
        loc.neededBytes = i + 4;
        return NdefLocate::NeedMore;
      }
      len = ((size_t)data[i + 2] << 8) | data[i + 3];
      header = 4;
    } else {
      len = data[i + 1];
      header = 2;
    }
    if (tag == 0x03) {
      if (len == 0) {
        return NdefLocate::None;
      }
      size_t end = i + header + len;
      if (end > avail) {
        loc.neededBytes = end;
        return NdefLocate::NeedMore;
      }
      loc.recordOffset = i + header;
      loc.recordLength = len;
      return NdefLocate::Found;
    }
    if (tag == 0x01 || tag == 0x02 || tag == 0xFD) {  // Lock Control, Memory Control, proprietär
      i += header + len;
      continue;
    }
    return NdefLocate::Invalid;
  }
  loc.neededBytes = avail + 16;
  return NdefLocate::NeedMore;
}

namespace {

struct RawRecord {
  uint8_t flags = 0;
  const uint8_t *type = nullptr;
  size_t typeLen = 0;
  const uint8_t *payload = nullptr;
  size_t payloadLen = 0;
  size_t total = 0;  // Länge des ganzen Records
};

bool parseRecord(const uint8_t *rec, size_t len, RawRecord &out) {
  if (len < 3) {
    return false;
  }
  out.flags = rec[0];
  bool sr = out.flags & 0x10, il = out.flags & 0x08;
  size_t pos = 1;
  out.typeLen = rec[pos++];
  if (sr) {
    out.payloadLen = rec[pos++];
  } else {
    if (len < pos + 4) {
      return false;
    }
    out.payloadLen =
        ((size_t)rec[pos] << 24) | ((size_t)rec[pos + 1] << 16) | ((size_t)rec[pos + 2] << 8) | rec[pos + 3];
    pos += 4;
  }
  size_t idLen = 0;
  if (il) {
    if (len < pos + 1) {
      return false;
    }
    idLen = rec[pos++];
  }
  if (out.payloadLen > len || pos + out.typeLen + idLen + out.payloadLen > len) {
    return false;
  }
  out.type = rec + pos;
  out.payload = rec + pos + out.typeLen + idLen;
  out.total = pos + out.typeLen + idLen + out.payloadLen;
  return true;
}

bool toMime(const RawRecord &r, NdefMime &out) {
  bool cf = r.flags & 0x20;
  if ((r.flags & 0x07) != 0x02 || cf || r.typeLen >= sizeof(out.type)) {
    return false;
  }
  memcpy(out.type, r.type, r.typeLen);
  out.type[r.typeLen] = '\0';
  out.payload = r.payload;
  out.payloadLength = r.payloadLen;
  return true;
}

}  // namespace

bool ndefParseMimeRecord(const uint8_t *rec, size_t len, NdefMime &out) {
  RawRecord r;
  return parseRecord(rec, len, r) && toMime(r, out);
}

bool ndefFindMimeRecord(const uint8_t *msg, size_t len, const char *mimeType, NdefMime &out) {
  size_t pos = 0;
  while (pos < len) {
    RawRecord r;
    if (!parseRecord(msg + pos, len - pos, r)) {
      return false;
    }
    NdefMime m;
    if (toMime(r, m) && strcmp(m.type, mimeType) == 0) {
      out = m;
      return true;
    }
    if (r.flags & 0x40) {  // ME: letzter Record
      return false;
    }
    pos += r.total;
  }
  return false;
}

size_t ndefBuildMimeTlv(const char *mimeType, const uint8_t *payload, size_t payloadLen, size_t capacityBytes,
                        uint8_t *out, size_t outCap) {
  size_t typeLen = strlen(mimeType);
  if (typeLen > 255) {
    return 0;
  }
  bool sr = payloadLen < 256;
  size_t recLen = 1 + 1 + (sr ? 1 : 4) + typeLen + payloadLen;
  size_t tlvHeader = recLen < 255 ? 2 : 4;
  size_t total = tlvHeader + recLen + 1;  // + Terminator
  size_t padded = (total + 3) & ~(size_t)3;
  if (padded > capacityBytes || padded > outCap) {
    return 0;
  }
  size_t p = 0;
  out[p++] = 0x03;
  if (recLen < 255) {
    out[p++] = (uint8_t)recLen;
  } else {
    out[p++] = 0xFF;
    out[p++] = (uint8_t)(recLen >> 8);
    out[p++] = (uint8_t)recLen;
  }
  out[p++] = sr ? 0xD2 : 0xC2;  // MB=1, ME=1, CF=0, SR, IL=0, TNF=MIME
  out[p++] = (uint8_t)typeLen;
  if (sr) {
    out[p++] = (uint8_t)payloadLen;
  } else {
    out[p++] = (uint8_t)(payloadLen >> 24);
    out[p++] = (uint8_t)(payloadLen >> 16);
    out[p++] = (uint8_t)(payloadLen >> 8);
    out[p++] = (uint8_t)payloadLen;
  }
  memcpy(out + p, mimeType, typeLen);
  p += typeLen;
  memcpy(out + p, payload, payloadLen);
  p += payloadLen;
  out[p++] = 0xFE;
  while (p < padded) {
    out[p++] = 0x00;
  }
  return padded;
}

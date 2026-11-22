#include "auth_throttle.h"

static bool idleExpired(const AuthThrottleEntry &e, uint32_t now) {
  // Unsigned-Differenz: richtig über den millis()-Überlauf hinweg
  return (uint32_t)(now - e.lastFail) >= AUTH_IDLE_FORGET_MS && (int32_t)(e.blockedUntil - now) <= 0;
}

static AuthThrottleEntry *find(AuthThrottle &t, uint32_t ip) {
  for (auto &e : t.slots) {
    if (e.used && e.ip == ip) return &e;
  }
  return nullptr;
}

uint32_t authThrottleLockMs(uint8_t n) {
  if (n == 0) return 0;
  uint32_t ms = AUTH_LOCK_BASE_MS;
  for (uint8_t k = 1; k < n && ms < AUTH_LOCK_MAX_MS; k++) ms *= 2;
  return ms < AUTH_LOCK_MAX_MS ? ms : AUTH_LOCK_MAX_MS;
}

uint32_t authThrottleBlockedMs(const AuthThrottle &t, uint32_t ip, uint32_t now) {
  for (const auto &e : t.slots) {
    if (e.used && e.ip == ip) {
      int32_t left = (int32_t)(e.blockedUntil - now);
      return left > 0 ? (uint32_t)left : 0;
    }
  }
  return 0;
}

uint32_t authThrottleFail(AuthThrottle &t, uint32_t ip, uint32_t now) {
  AuthThrottleEntry *e = find(t, ip);
  if (e && idleExpired(*e, now)) {
    *e = AuthThrottleEntry();
    e->used = true;
    e->ip = ip;
  }
  if (!e) {
    // Freier oder abgelaufener Platz, sonst der am längsten ruhige (ungesperrte bevorzugt)
    AuthThrottleEntry *victim = nullptr;
    bool victimBlocked = true;
    for (auto &s : t.slots) {
      if (!s.used || idleExpired(s, now)) {
        victim = &s;
        break;
      }
      bool blocked = (int32_t)(s.blockedUntil - now) > 0;
      if (!victim || (victimBlocked && !blocked) ||
          (blocked == victimBlocked && (uint32_t)(now - s.lastFail) > (uint32_t)(now - victim->lastFail))) {
        victim = &s;
        victimBlocked = blocked;
      }
    }
    *victim = AuthThrottleEntry();
    victim->used = true;
    victim->ip = ip;
    e = victim;
  }
  e->lastFail = now;
  if (++e->fails < AUTH_FAILS_PER_LOCK) return 0;
  e->fails = 0;
  if (e->locks < 255) e->locks++;
  uint32_t ms = authThrottleLockMs(e->locks);
  e->blockedUntil = now + ms;
  return ms;
}

void authThrottleSuccess(AuthThrottle &t, uint32_t ip) {
  AuthThrottleEntry *e = find(t, ip);
  if (e) *e = AuthThrottleEntry();
}

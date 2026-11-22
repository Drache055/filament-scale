#include "devmode.h"
#include "util/log.h"

#include <Preferences.h>

#include "config.h"

namespace {
volatile bool simpleMode = false;
volatile int emptySpoolG = WEIGH_DEFAULT_EMPTY_G;
}  // namespace

void devModeBegin() {
  Preferences p;
  p.begin("devmode", false);
  simpleMode = p.getBool("simple", false);
  emptySpoolG = p.getInt("empty", WEIGH_DEFAULT_EMPTY_G);
  p.end();
  logPrintf("[MODE] %s\n", simpleMode ? "Einfache Waage" : "Smarte Waage");
}

bool devModeSimple() { return simpleMode; }

void devModeSetSimple(bool simple) {
  if (simple == simpleMode) {
    return;
  }
  simpleMode = simple;
  Preferences p;
  p.begin("devmode", false);
  p.putBool("simple", simple);
  p.end();
  logPrintf("[MODE] Umgeschaltet: %s\n", simple ? "Einfache Waage" : "Smarte Waage");
}

int devModeEmptySpoolG() { return emptySpoolG; }

void devModeSetEmptySpoolG(int grams) {
  if (grams < 0) grams = 0;
  if (grams > 2000) grams = 2000;
  if (grams == emptySpoolG) {
    return;
  }
  emptySpoolG = grams;
  Preferences p;
  p.begin("devmode", false);
  p.putInt("empty", grams);
  p.end();
}

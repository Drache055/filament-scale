#include "util/log.h"

#include <Arduino.h>
#include <lvgl.h>

#include "app/session.h"
#include "config.h"
#include "core/simple_core.h"
#include "i18n/i18n.h"
#include "scale/scale.h"
#include "screens.h"
#include "storage/devmode.h"
#include "theme.h"

// Einfache Waage (CLAUDE.md 5.4): "Filament wiegen" ohne Spoolman, ohne Anmeldung und ohne NFC.
// Der Zustandsautomat liegt in core/simple_core, hier stehen nur Screens und Anbindung.
// Eigener Namespace (nicht anonym), weil das Screenshot-Tool alle Screen-Dateien in eine Übersetzungseinheit einbindet.
namespace simpleui {

struct Ctx {
  SimpleFlow flow;
  int emptySpool = WEIGH_DEFAULT_EMPTY_G;
  SimpleState shown = SimpleState::Idle;
  lv_timer_t *timer = nullptr;
  bool reqCancel = false;
  bool reqTakeBox = false;

  // Widgets des aktuellen Screens
  lv_obj_t *weight = nullptr;
  lv_obj_t *gross = nullptr;
  lv_obj_t *info = nullptr;
  lv_obj_t *emptyLabel = nullptr;
  lv_obj_t *tareBtn = nullptr;
};

Ctx *ctx = nullptr;

lv_obj_t *label(lv_obj_t *parent, const lv_style_t *style, const char *text = "") {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_add_style(l, style, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, LV_PCT(100));
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(l, text);
  return l;
}

lv_obj_t *card(lv_obj_t *scr) {
  lv_obj_t *c = lv_obj_create(scr);
  lv_obj_remove_style_all(c);
  lv_obj_add_style(c, &theme().card, 0);
  lv_obj_set_width(c, LV_PCT(100));
  lv_obj_set_flex_grow(c, 1);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(c, themePadding(), 0);
  return c;
}

lv_obj_t *row(lv_obj_t *scr) {
  lv_obj_t *r = lv_obj_create(scr);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, LV_PCT(100), themeButtonHeight());
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(r, themePadding(), 0);
  return r;
}

void title(lv_obj_t *scr, const char *text) { label(scr, &theme().body, text); }

void onCancel(lv_event_t *) { if (ctx) ctx->reqCancel = true; }
void onTakeBox(lv_event_t *) { if (ctx) ctx->reqTakeBox = true; }
void adjustEmpty(int delta) {
  if (!ctx) return;
  ctx->emptySpool += delta;
  if (ctx->emptySpool < 0) ctx->emptySpool = 0;
  if (ctx->emptySpool > 2000) ctx->emptySpool = 2000;
}
void onEmptyMinus(lv_event_t *) { adjustEmpty(-10); }
void onEmptyPlus(lv_event_t *) { adjustEmpty(10); }

void build() {
  ctx->weight = ctx->gross = ctx->info = ctx->emptyLabel = ctx->tareBtn = nullptr;
  lv_obj_t *scr = uiNewScreen();

  switch (ctx->flow.state()) {
    case SimpleState::Clear: {
      title(scr, tr(Str::WfEmptyScale));
      lv_obj_t *c = card(scr);
      ctx->weight = label(c, &theme().weight);
      lv_obj_t *r = row(scr);
      uiMakeButton(r, tr(Str::WfCancel), onCancel);
      break;
    }
    case SimpleState::WaitBox: {
      title(scr, tr(Str::SwPlaceBox));
      lv_obj_t *c = card(scr);
      ctx->weight = label(c, &theme().weight);
      ctx->info = label(c, &theme().dim, tr(Str::SwBoxHint));
      lv_obj_t *r = row(scr);
      ctx->tareBtn = uiMakeButton(r, tr(Str::SwTareBox), onTakeBox);
      lv_obj_set_style_opa(ctx->tareBtn, LV_OPA_40, LV_STATE_DISABLED);
      lv_obj_add_state(ctx->tareBtn, LV_STATE_DISABLED);
      lv_obj_t *cancel = uiMakeButton(r, LV_SYMBOL_CLOSE, onCancel);  // schmal: eine Hauptaktion
      lv_obj_set_flex_grow(cancel, 0);
      lv_obj_set_width(cancel, themeButtonHeight());
      break;
    }
    case SimpleState::Measure: {
      title(scr, ctx->flow.withBox() ? tr(Str::SwSpoolInBox) : tr(Str::SwPlaceSpool));
      lv_obj_t *c = card(scr);
      label(c, &theme().dim, tr(Str::Filament));
      ctx->weight = label(c, &theme().weight);
      ctx->gross = label(c, &theme().dim);  // Spule gesamt (mit Box: und Box-Gewicht)
      lv_obj_t *er = row(scr);
      lv_obj_t *minus = uiMakeButton(er, "-10", onEmptyMinus);
      lv_obj_set_flex_grow(minus, 0);
      lv_obj_set_width(minus, themeButtonHeight());
      lv_obj_t *mid = lv_label_create(er);
      lv_obj_add_style(mid, &theme().dim, 0);  // zweizeilig, passt zwischen die schmalen Buttons
      lv_obj_set_flex_grow(mid, 1);
      lv_obj_set_style_text_align(mid, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_pad_top(mid, themeButtonHeight() / 8, 0);
      ctx->emptyLabel = mid;
      lv_obj_t *plus = uiMakeButton(er, "+10", onEmptyPlus);
      lv_obj_set_flex_grow(plus, 0);
      lv_obj_set_width(plus, themeButtonHeight());
      lv_obj_t *r = row(scr);
      uiMakeButton(r, tr(Str::WfDone), onCancel);
      break;
    }
    default:
      break;
  }
  uiNavigate(scr);
}

void setWeight(lv_obj_t *l, long g, bool stable) {
  if (!l) return;
  lv_label_set_text_fmt(l, "%ld g", g == 0 ? 0L : g);
  lv_obj_set_style_text_color(l, stable ? themeColors().text : themeColors().dim, 0);
}

void refreshLive(const ScaleReading &r, const SimpleInput &in) {
  const ThemeColors &c = themeColors();
  switch (ctx->flow.state()) {
    case SimpleState::Clear:
      setWeight(ctx->weight, lroundf(r.grams), in.stable);
      break;
    case SimpleState::WaitBox: {
      setWeight(ctx->weight, lroundf(r.grams - ctx->flow.zero()), in.stable);
      if (ctx->flow.canTakeBox(in)) {
        lv_obj_remove_state(ctx->tareBtn, LV_STATE_DISABLED);
      } else {
        lv_obj_add_state(ctx->tareBtn, LV_STATE_DISABLED);
      }
      break;
    }
    case SimpleState::Measure: {
      float gross = ctx->flow.spoolGross(r.grams);
      setWeight(ctx->weight, lroundf(simpleFilament(gross, (float)ctx->emptySpool)), in.stable);
      if (r.overload) {
        lv_label_set_text(ctx->gross, tr(Str::Overload));
        lv_obj_set_style_text_color(ctx->gross, c.error, 0);
      } else {
        if (ctx->flow.withBox()) {
          lv_label_set_text_fmt(ctx->gross, tr(Str::SwGrossBox), (int)lroundf(gross), (int)lroundf(ctx->flow.boxWeight()));
        } else {
          lv_label_set_text_fmt(ctx->gross, tr(Str::SwGross), (int)lroundf(gross));
        }
        lv_obj_set_style_text_color(ctx->gross, c.dim, 0);
      }
      lv_label_set_text_fmt(ctx->emptyLabel, "%s\n%d g", tr(Str::WfEmptySpool), ctx->emptySpool);
      break;
    }
    default:
      break;
  }
}

void release() {
  devModeSetEmptySpoolG(ctx->emptySpool);  // zuletzt eingestellte Leerspule merken
  lv_timer_delete(ctx->timer);
  delete ctx;
  ctx = nullptr;
}

void tick(lv_timer_t *) {
  if (!ctx) return;
  ScaleReading r = scaleGet();
  SimpleInput in;
  in.now = millis();
  in.grams = r.grams;
  in.stable = r.valid && r.calibrated && r.stable;
  in.empty = in.stable && fabsf(r.grams) <= SCALE_EMPTY_G;
  in.cancel = ctx->reqCancel;
  in.takeBox = ctx->reqTakeBox;
  ctx->reqCancel = ctx->reqTakeBox = false;

  SimpleState before = ctx->flow.state();
  ctx->flow.step(in);
  SimpleState after = ctx->flow.state();
  if (after == SimpleState::Idle) {
    SimpleEnd why = ctx->flow.end();
    release();
    uiShowHome();
    if (why == SimpleEnd::Timeout) {
      sessionPostNotice(tr(Str::WfTimeout), 1);
    }
    return;
  }
  if (after != before) {
    if (after == SimpleState::Measure) {
      logPrintf("[SIMPLE] Referenz %.1f g (%s)\n", ctx->flow.reference(), ctx->flow.withBox() ? "Box" : "Nullpunkt");
    }
    build();
  }
  refreshLive(r, in);
}

void startFlow(bool withBox) {
  if (ctx) return;
  ctx = new Ctx();
  SimpleConfig cfg;
  cfg.addedG = WEIGH_ADDED_G;
  cfg.timeoutMs = WEIGH_TIMEOUT_MS;
  ctx->flow = SimpleFlow(cfg);
  ctx->emptySpool = devModeEmptySpoolG();
  ctx->flow.start(millis(), withBox);
  logPrintf("[SIMPLE] Start: %s\n", withBox ? "Box + Spule" : "Nur Spule");
  build();
  ctx->timer = lv_timer_create(tick, 200, nullptr);
  tick(nullptr);  // ist die Waage schon leer, geht es sofort weiter
}

void onSpoolOnly(lv_event_t *) { startFlow(false); }
void onBoxSpool(lv_event_t *) { startFlow(true); }

}  // namespace simpleui

bool simpleFlowActive() { return simpleui::ctx != nullptr; }

void simpleFlowAbort() {
  if (!simpleui::ctx) return;
  simpleui::release();
}

void screenSimpleWeighOpen() {
  using namespace simpleui;
  lv_obj_t *scr = uiNewScreen();
  title(scr, tr(Str::SwWeigh));
  lv_obj_t *c = card(scr);
  label(c, &theme().dim, tr(Str::SwChoose));
  uiMakeButton(c, tr(Str::SwSpoolOnly), onSpoolOnly, true);
  uiMakeButton(c, tr(Str::SwBoxSpool), onBoxSpool, true);
  lv_obj_t *r = row(scr);
  uiMakeButton(r, tr(Str::Back), [](lv_event_t *) { uiShowHome(); });
  uiNavigate(scr);
}

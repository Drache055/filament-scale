#include "util/log.h"

#include <Arduino.h>
#include <lvgl.h>

#include "app/session.h"
#include "config.h"
#include "core/weigh_core.h"
#include "i18n/i18n.h"
#include "net/spoolman.h"
#include "nfc/nfc.h"
#include "scale/scale.h"
#include "screens.h"
#include "theme.h"
#include "ui.h"

// Wiegeablauf (CLAUDE.md 5): Der Zustandsautomat liegt in core/weigh_core, hier stehen nur Screens und Anbindung.
// Ein Timer (200 ms) wertet Waage und Tag aus und baut bei jedem Zustandswechsel den Screen neu auf.
namespace {

struct Ctx {
  WeighFlow flow;
  SpoolInfo spool;
  char owner[24] = "";
  char boxName[65] = "";
  float emptySpool = -1;      // aus Spoolman oder manuell
  bool emptyManual = false;
  WeighState shown = WeighState::Idle;
  lv_timer_t *timer = nullptr;
  uint32_t writeSeq = 0;      // Ergebnis-Zähler vor dem Senden
  bool queued = false;
  char error[96] = "";
  float savedRemaining = 0;
  // Eingaben aus Button-Callbacks
  bool reqCancel = false;
  bool reqSave = false;

  // Widgets des aktuellen Screens
  lv_obj_t *weight = nullptr;
  lv_obj_t *info = nullptr;
  lv_obj_t *hint = nullptr;
  lv_obj_t *emptyLabel = nullptr;
  lv_obj_t *saveBtn = nullptr;
};

Ctx *ctx = nullptr;

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_style_t *style, bool wrap = true) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_add_style(l, style, 0);
  if (wrap) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  }
  return l;
}

lv_obj_t *makeCard(lv_obj_t *scr) {
  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_remove_style_all(card);
  lv_obj_add_style(card, &theme().card, 0);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_flex_grow(card, 1);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, themePadding(), 0);
  return card;
}

lv_obj_t *makeRow(lv_obj_t *scr) {
  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), themeButtonHeight());
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, themePadding(), 0);
  return row;
}

void onCancel(lv_event_t *) { if (ctx) ctx->reqCancel = true; }
void onSave(lv_event_t *) { if (ctx) ctx->reqSave = true; }

void adjustEmpty(int delta) {
  if (!ctx) return;
  ctx->emptySpool += delta;
  if (ctx->emptySpool < 0) ctx->emptySpool = 0;
}
void onEmptyMinus(lv_event_t *) { adjustEmpty(-10); }
void onEmptyPlus(lv_event_t *) { adjustEmpty(10); }

lv_color_t spoolColor() {
  const char *hex = ctx->spool.colorHex;
  return lv_color_hex(hex[0] ? (uint32_t)strtoul(hex, nullptr, 16) : 0xFFFFFF);
}

void spoolName(char *out, size_t cap) {
  snprintf(out, cap, "%s %s", ctx->spool.vendor, ctx->spool.material);
}

WeighResult currentResult() {
  return weighCompute(ctx->flow.w1(), ctx->flow.w2(), ctx->emptySpool, ctx->spool.initialWeight);
}

void setTitle(lv_obj_t *scr, const char *text, lv_color_t color) {
  lv_obj_t *t = makeLabel(scr, &theme().body);
  lv_label_set_text(t, text);
  lv_obj_set_style_text_color(t, color, 0);
}

// Baut den Screen zum aktuellen Zustand
void build() {
  WeighState st = ctx->flow.state();
  ctx->weight = ctx->info = ctx->hint = ctx->emptyLabel = ctx->saveBtn = nullptr;
  lv_obj_t *scr = uiNewScreen();
  const ThemeColors &c = themeColors();
  char buf[96];

  switch (st) {
    case WeighState::SpoolIdentified: {
      setTitle(scr, tr(Str::WfSpoolFound), c.text);
      lv_obj_t *card = makeCard(scr);
      lv_obj_t *sw = lv_obj_create(card);
      lv_obj_remove_style_all(sw);
      lv_obj_set_size(sw, 80, 28);
      lv_obj_set_style_radius(sw, 6, 0);
      lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(sw, spoolColor(), 0);
      spoolName(buf, sizeof(buf));
      lv_label_set_text(makeLabel(card, &theme().title), buf);
      if (ctx->spool.remaining >= 0) {
        lv_label_set_text_fmt(makeLabel(card, &theme().dim), tr(Str::WfLastRemaining), (int)lroundf(ctx->spool.remaining));
      }
      ctx->hint = makeLabel(card, &theme().body);
      break;
    }
    case WeighState::WaitBox: {
      setTitle(scr, tr(Str::WfPlaceBox), c.text);
      lv_obj_t *card = makeCard(scr);
      ctx->weight = makeLabel(card, &theme().weight);
      ctx->info = makeLabel(card, &theme().body);
      break;
    }
    case WeighState::WaitSpool: {
      setTitle(scr, tr(Str::WfPlaceSpool), c.text);
      lv_obj_t *card = makeCard(scr);
      ctx->weight = makeLabel(card, &theme().weight);
      snprintf(buf, sizeof(buf), "%s: %s, %d g", tr(Str::KindBox), ctx->boxName, (int)lroundf(ctx->flow.w1()));
      lv_label_set_text(makeLabel(card, &theme().dim), buf);
      break;
    }
    case WeighState::Confirm: {
      setTitle(scr, tr(Str::WfConfirm), c.text);
      lv_obj_t *card = makeCard(scr);
      snprintf(buf, sizeof(buf), "%s: %s", tr(Str::KindBox), ctx->boxName);
      lv_label_set_text(makeLabel(card, &theme().body), buf);
      spoolName(buf, sizeof(buf));
      lv_label_set_text(makeLabel(card, &theme().body), buf);
      lv_label_set_text(makeLabel(card, &theme().dim), tr(Str::WfRemaining));
      ctx->weight = makeLabel(card, &theme().weight);
      ctx->hint = makeLabel(card, &theme().dim);
      if (ctx->emptyManual) {  // kein Leerspulengewicht in Spoolman: Wert von Hand einstellen
        lv_obj_t *row = makeRow(scr);
        uiMakeButton(row, "-10", onEmptyMinus);
        lv_obj_t *mid = lv_label_create(row);
        lv_obj_add_style(mid, &theme().body, 0);
        lv_obj_set_flex_grow(mid, 2);
        lv_obj_set_style_text_align(mid, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(mid, themeButtonHeight() / 4, 0);
        ctx->emptyLabel = mid;
        uiMakeButton(row, "+10", onEmptyPlus);
      }
      lv_obj_t *row = makeRow(scr);
      ctx->saveBtn = uiMakeButton(row, tr(Str::Save), onSave);
      lv_obj_t *cancel = uiMakeButton(row, LV_SYMBOL_CLOSE, onCancel);  // schmal, damit "Speichern" Platz hat
      lv_obj_set_flex_grow(cancel, 0);
      lv_obj_set_width(cancel, themeButtonHeight());
      uiNavigate(scr);
      return;
    }
    case WeighState::Upload: {
      setTitle(scr, tr(Str::WfSaving), c.text);
      makeCard(scr);
      uiNavigate(scr);
      return;  // kein Abbrechen während des Sendens
    }
    case WeighState::Done: {
      setTitle(scr, ctx->queued ? tr(Str::WfQueued) : tr(Str::Saved), ctx->queued ? c.warn : c.ok);
      lv_obj_t *card = makeCard(scr);
      lv_label_set_text_fmt(makeLabel(card, &theme().weight), "%d g", (int)lroundf(ctx->savedRemaining));
      snprintf(buf, sizeof(buf), "%s, %s", ctx->boxName, ctx->spool.vendor);
      lv_label_set_text(makeLabel(card, &theme().dim), buf);
      lv_label_set_text(makeLabel(card, &theme().title), tr(Str::WfRemoveBox));
      lv_obj_t *row = makeRow(scr);
      uiMakeButton(row, tr(Str::WfDone), onCancel);
      uiNavigate(scr);
      return;
    }
    case WeighState::Failed: {
      setTitle(scr, tr(Str::WfSaveFailed), c.error);
      lv_obj_t *card = makeCard(scr);
      lv_obj_t *e = makeLabel(card, &theme().body);
      lv_label_set_text(e, ctx->error);
      lv_obj_set_style_text_color(e, c.error, 0);
      lv_obj_t *row = makeRow(scr);
      uiMakeButton(row, tr(Str::WfRetry), onSave);
      uiMakeButton(row, tr(Str::WfCancel), onCancel);
      uiNavigate(scr);
      return;
    }
    default:
      break;
  }
  lv_obj_t *row = makeRow(scr);
  uiMakeButton(row, tr(Str::WfCancel), onCancel);
  uiNavigate(scr);
}

void setHint(const char *text, lv_color_t color) {
  if (ctx->hint) {
    lv_label_set_text(ctx->hint, text);
    lv_obj_set_style_text_color(ctx->hint, color, 0);
  }
}

// Live-Anzeigen des aktuellen Zustands
void refreshLive(const ScaleReading &r, const NfcState &n) {
  const ThemeColors &c = themeColors();
  long g = lroundf(r.grams);
  switch (ctx->flow.state()) {
    case WeighState::SpoolIdentified:
      if (r.empty) {
        setHint("", c.text);
      } else {
        setHint(tr(Str::WfEmptyScale), c.warn);
      }
      break;
    case WeighState::WaitBox:
      lv_label_set_text_fmt(ctx->weight, "%ld g", g == 0 ? 0L : g);
      if (n.tagPresent && n.readSeq == n.seq && n.content.kind == TagKind::Box) {
        snprintf(ctx->boxName, sizeof(ctx->boxName), "%s", n.content.box.name);
        char buf[96];
        snprintf(buf, sizeof(buf), "%s: %s", tr(Str::KindBox), ctx->boxName);
        lv_label_set_text(ctx->info, buf);
        lv_obj_set_style_text_color(ctx->info, c.ok, 0);
      } else {
        lv_label_set_text(ctx->info, tr(Str::WfSearchBoxTag));
        lv_obj_set_style_text_color(ctx->info, c.dim, 0);
      }
      break;
    case WeighState::WaitSpool: {
      long rel = lroundf(r.grams - ctx->flow.w1());
      lv_label_set_text_fmt(ctx->weight, "%ld g", rel < 0 ? 0L : rel);
      break;
    }
    case WeighState::Confirm: {
      WeighResult res = currentResult();
      lv_label_set_text_fmt(ctx->weight, "%d g", (int)lroundf(res.remaining));
      lv_obj_set_style_text_color(ctx->weight, res.check == WeighCheck::EmptyTooBig ? c.error : c.text, 0);
      switch (res.check) {
        case WeighCheck::EmptyTooBig: setHint(tr(Str::WfCheckEmpty), c.error); break;
        case WeighCheck::Clamped: setHint(tr(Str::WfClamped), c.warn); break;
        case WeighCheck::TooHeavy: setHint(tr(Str::WfTooHeavy), c.warn); break;
        default: setHint("", c.text); break;
      }
      if (ctx->saveBtn) {
        if (res.check == WeighCheck::EmptyTooBig) {
          lv_obj_add_flag(ctx->saveBtn, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_remove_flag(ctx->saveBtn, LV_OBJ_FLAG_HIDDEN);
        }
      }
      if (ctx->emptyLabel) {
        lv_label_set_text_fmt(ctx->emptyLabel, "%s: %d g", tr(Str::WfEmptySpool), (int)lroundf(ctx->emptySpool));
      }
      break;
    }
    default:
      break;
  }
}

void endFlow(WeighEnd why) {
  lv_timer_delete(ctx->timer);
  delete ctx;
  ctx = nullptr;
  uiShowHome();
  if (why == WeighEnd::Timeout) {
    sessionPostNotice(tr(Str::WfTimeout), 1);
  }
}

void tick(lv_timer_t *) {
  if (!ctx) return;
  ScaleReading r = scaleGet();
  NfcState n = nfcGet();
  WeighInput in;
  in.now = millis();
  in.grams = r.grams;
  in.stable = r.valid && r.calibrated && r.stable;
  in.empty = in.stable && fabsf(r.grams) <= SCALE_EMPTY_G;
  in.boxTag = n.tagPresent && n.readSeq == n.seq && n.content.kind == TagKind::Box && n.content.box.name[0];
  in.cancel = ctx->reqCancel;
  in.save = ctx->reqSave;
  ctx->reqCancel = ctx->reqSave = false;

  WeighState before = ctx->flow.state();
  if (before == WeighState::Upload) {
    SpoolmanResult w = spoolmanWriteResult();
    if (w.seq != ctx->writeSeq) {
      if (w.state == SpoolmanState::Found || w.queued) {
        ctx->queued = w.queued;
        in.uploadDone = true;
      } else {
        strlcpy(ctx->error, w.error, sizeof(ctx->error));
        in.uploadFailed = true;
      }
    }
  }
  if (before == WeighState::WaitBox && in.boxTag) {
    snprintf(ctx->boxName, sizeof(ctx->boxName), "%s", n.content.box.name);  // Box-Name wird beim Einfrieren von W1 festgelegt
  }
  ctx->flow.step(in);

  WeighState after = ctx->flow.state();
  if (after == WeighState::Idle) {
    endFlow(ctx->flow.end());
    return;
  }
  if (after != before) {
    if (after == WeighState::Confirm) {
      ctx->emptyManual = ctx->spool.emptySpool < 0;
      ctx->emptySpool = ctx->emptyManual ? WEIGH_DEFAULT_EMPTY_G : ctx->spool.emptySpool;
    }
    if (after == WeighState::Upload) {
      WeighResult res = currentResult();
      ctx->savedRemaining = res.remaining;
      ctx->writeSeq = spoolmanWriteResult().seq;
      spoolmanRequestUpdate(ctx->spool.id, res.remaining, ctx->boxName, ctx->owner[0] ? ctx->owner : nullptr);
      logPrintf("[FLOW] Spule %d: W1 %.1f, W2 %.1f, Leerspule %.0f -> Rest %.1f g, Box %s\n", ctx->spool.id,
                ctx->flow.w1(), ctx->flow.w2(), ctx->emptySpool, res.remaining, ctx->boxName);
    }
    build();
  }
  refreshLive(r, n);
}

}  // namespace

bool weighFlowActive() { return ctx != nullptr; }

void weighFlowAbort() {
  if (!ctx) return;
  lv_timer_delete(ctx->timer);
  delete ctx;
  ctx = nullptr;
}

void weighFlowStart(const SpoolInfo &spool, const char *owner) {
  if (ctx) return;
  ctx = new Ctx();
  WeighConfig cfg;
  cfg.addedG = WEIGH_ADDED_G;
  cfg.timeoutMs = WEIGH_TIMEOUT_MS;
  cfg.showSpoolMs = WEIGH_SHOW_SPOOL_MS;
  ctx->flow = WeighFlow(cfg);
  ctx->spool = spool;
  strlcpy(ctx->owner, owner ? owner : "", sizeof(ctx->owner));
  ctx->flow.start(millis());
  logPrintf("[FLOW] Start mit Spule %d (%s %s)\n", spool.id, spool.vendor, spool.material);
  build();
  ctx->timer = lv_timer_create(tick, 200, nullptr);
  refreshLive(scaleGet(), nfcGet());
}

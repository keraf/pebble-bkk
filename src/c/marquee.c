#include "marquee.h"

#define START_DELAY_MS 900
#define END_HOLD_MS 1200
#define TICK_MS 60
#define STEP_PX 2

typedef enum {
  PhaseIdle,      // selected text fits, or nothing measured yet
  PhaseWaiting,   // overflow detected, delay before scrolling starts
  PhaseScrolling,
  PhaseEndHold,   // fully scrolled, hold before snapping back
} Phase;

static MarqueeDirtyCallback s_dirty;
static AppTimer *s_timer;
static Phase s_phase = PhaseIdle;
static int16_t s_offset;
static int16_t s_overflow;

static void prv_tick(void *data);

static void prv_schedule(uint32_t ms) {
  if (s_timer) {
    app_timer_cancel(s_timer);
  }
  s_timer = app_timer_register(ms, prv_tick, NULL);
}

static void prv_tick(void *data) {
  s_timer = NULL;
  switch (s_phase) {
    case PhaseWaiting:
      s_phase = PhaseScrolling;
      /* fall through */
    case PhaseScrolling:
      s_offset += STEP_PX;
      if (s_offset >= s_overflow) {
        s_offset = s_overflow;
        s_phase = PhaseEndHold;
        prv_schedule(END_HOLD_MS);
      } else {
        prv_schedule(TICK_MS);
      }
      break;
    case PhaseEndHold:
      s_offset = 0;
      s_phase = PhaseWaiting;
      prv_schedule(START_DELAY_MS);
      break;
    default:
      return;
  }
  if (s_dirty) {
    s_dirty();
  }
}

void marquee_reset(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  s_phase = PhaseIdle;
  s_offset = 0;
  s_overflow = 0;
}

void marquee_stop(void) {
  marquee_reset();
}

void marquee_set_callback(MarqueeDirtyCallback cb) {
  marquee_reset();
  s_dirty = cb;
}

// Called from the selected row's draw with its current overflow.
static void prv_report_overflow(int16_t overflow) {
  s_overflow = overflow;
  if (overflow <= 0) {
    if (s_phase != PhaseIdle) {
      marquee_reset();
    }
    return;
  }
  if (s_phase == PhaseIdle) {
    s_phase = PhaseWaiting;
    prv_schedule(START_DELAY_MS);
  }
}

void marquee_draw_text(GContext *ctx, GRect area, const char *text,
                       const char *font_key, bool selected, GColor color) {
  GFont font = fonts_get_system_font(font_key);
  bool scrolling = false;

  if (selected) {
    GSize size = graphics_text_layout_get_content_size(
        text, font, GRect(0, 0, 2000, area.size.h),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    int16_t overflow = size.w - area.size.w;
    prv_report_overflow(overflow > 0 ? overflow : 0);
    scrolling = overflow > 0;
  }

  graphics_context_set_text_color(ctx, color);
  GRect box = area;
  if (scrolling) {
    box.origin.x -= s_offset;
    box.size.w += s_overflow + 4;
  }
  graphics_draw_text(ctx, text, font, box,
                     scrolling ? GTextOverflowModeWordWrap
                               : GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

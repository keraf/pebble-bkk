#include "departures_window.h"

#include <stdio.h>
#include <string.h>

#include "favorites.h"
#include "locale.h"
#include "marquee.h"
#include "transit_types.h"

#define CELL_HEIGHT 44

static Window *s_window;
static MenuLayer *s_menu;

static char s_stop_id[96];
static char s_stop_name[32];
static char s_stop_badge[8];
static uint8_t s_stop_type;
static int32_t s_stop_color;
static int32_t s_stop_textcol;

static CommItem s_deps[COMM_MAX_ITEMS];
static int s_count;
static bool s_loading;
static int s_error;

static int s_refresh_secs = 60;
static AppTimer *s_refresh_timer;

static void prv_schedule_refresh(void);

static void prv_refresh_timer_cb(void *data) {
  s_refresh_timer = NULL;
  comm_request_departures(s_stop_id);
  // Next cycle is scheduled when the phone answers (data or error), so a dead
  // phone connection never turns into a hot request loop.
}

static void prv_schedule_refresh(void) {
  if (s_refresh_timer) {
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
  if (s_refresh_secs > 0 && departures_window_is_top()) {
    s_refresh_timer = app_timer_register(s_refresh_secs * 1000,
                                         prv_refresh_timer_cb, NULL);
  }
}

static void prv_format_eta(int32_t departure, int32_t realtime, char *buf,
                           size_t len) {
  int diff = (int)(departure - (int32_t)time(NULL));
  const char *tilde = realtime ? "" : "~"; // ~ marks schedule-only times
  if (diff <= 0) {
    snprintf(buf, len, "%s", tr(StrEtaNow));
  } else if (diff < 60) {
    snprintf(buf, len, tr(StrEtaSecondsFmt), tilde, diff);
  } else if (diff < 120) {
    snprintf(buf, len, tr(StrEtaOneMinuteFmt), tilde);
  } else if (diff < 3600) {
    snprintf(buf, len, tr(StrEtaMinutesFmt), tilde, diff / 60);
  } else {
    snprintf(buf, len, "%s", tr(StrEtaOverHour));
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

static uint16_t prv_get_num_sections(MenuLayer *menu_layer, void *context) {
  return 1;
}

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *context) {
  if (s_loading || s_error) {
    return 1;
  }
  return s_count;
}

static int16_t prv_get_header_height(MenuLayer *menu_layer,
                                     uint16_t section_index, void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static int16_t prv_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index,
                                   void *context) {
  return CELL_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer,
                            uint16_t section_index, void *context) {
  char header[40];
  snprintf(header, sizeof(header), "%s%s",
           favorites_contains(s_stop_id) ? "* " : "", s_stop_name);
  transit_draw_menu_header(ctx, cell_layer, header);
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *cell_index, void *context) {
  if (s_loading) {
    menu_cell_basic_draw(ctx, cell_layer, tr(StrLoading), tr(StrFetchingDepartures), NULL);
    return;
  }
  if (s_error) {
    const char *title;
    const char *subtitle;
    comm_error_strings(s_error, &title, &subtitle);
    menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
    return;
  }
  if (cell_index->row >= s_count) {
    return;
  }

  const CommItem *dep = &s_deps[cell_index->row];
  GRect bounds = layer_get_bounds((Layer *)cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted((Layer *)cell_layer);
  GColor highlight_bg = PBL_IF_COLOR_ELSE(GColorFromHEX(0x522E91), GColorBlack);

  GRect badge = GRect(4, (bounds.size.h - 28) / 2, 34, 28);
  char eta[32]; // fits "~59 másodperc múlva"
  prv_format_eta(dep->time, dep->extra, eta, sizeof(eta));

  int text_x = badge.origin.x + badge.size.w + 6;
  int text_w = bounds.size.w - text_x - 4;

  // Line 1: destination across the full width (marquee-scrolls when
  // selected). Line 2: the countdown. The badge is drawn after the headsign
  // so it covers any marquee overshoot on the left.
  marquee_draw_text(ctx, GRect(text_x, 2, text_w, 20), dep->sub,
                    FONT_KEY_GOTHIC_14_BOLD, highlighted,
                    highlighted ? GColorWhite : GColorBlack);

  GColor eta_color = highlighted ? GColorWhite : GColorBlack;
#if defined(PBL_COLOR)
  if (!highlighted && dep->extra) {
    eta_color = GColorDarkGreen; // realtime prediction
  }
#endif
  graphics_context_set_text_color(ctx, eta_color);
  graphics_draw_text(ctx, eta, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(text_x, 18, text_w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (highlighted) {
    graphics_context_set_fill_color(ctx, highlight_bg);
    graphics_fill_rect(ctx, GRect(0, 0, text_x, bounds.size.h), 0, GCornerNone);
  }
  transit_draw_badge(ctx, badge, dep->name, dep->type, dep->color,
                     dep->textcol, highlighted);
}

static void prv_selection_changed(MenuLayer *menu_layer, MenuIndex new_index,
                                  MenuIndex old_index, void *context) {
  marquee_reset();
}

static void prv_mark_dirty(void) {
  if (s_menu) {
    layer_mark_dirty(menu_layer_get_layer(s_menu));
  }
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index,
                             void *context) {
  // Manual refresh; the phone side throttles duplicates to protect the API key.
  comm_request_departures(s_stop_id);
}

static void prv_select_long_click(MenuLayer *menu_layer, MenuIndex *cell_index,
                                  void *context) {
  Favorite fav;
  memset(&fav, 0, sizeof(fav));
  strncpy(fav.id, s_stop_id, sizeof(fav.id) - 1);
  strncpy(fav.name, s_stop_name, sizeof(fav.name) - 1);
  strncpy(fav.badge, s_stop_badge, sizeof(fav.badge) - 1);
  fav.type = s_stop_type;
  fav.color = s_stop_color;
  fav.textcol = s_stop_textcol;

  favorites_toggle(&fav);
  vibes_short_pulse();
  if (s_menu) {
    menu_layer_reload_data(s_menu); // header star reflects the new state
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_get_num_sections,
    .get_num_rows = prv_get_num_rows,
    .get_header_height = prv_get_header_height,
    .get_cell_height = prv_get_cell_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
    .select_long_click = prv_select_long_click,
    .selection_changed = prv_selection_changed,
  });
#if defined(PBL_COLOR)
  menu_layer_set_highlight_colors(s_menu, GColorFromHEX(0x522E91), GColorWhite);
#endif
#if defined(PBL_ROUND)
  menu_layer_set_center_focused(s_menu, true);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu));
}

static void prv_window_appear(Window *window) {
  marquee_set_callback(prv_mark_dirty);
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
  comm_request_departures(s_stop_id);
  prv_schedule_refresh();
}

static void prv_window_disappear(Window *window) {
  marquee_stop();
  tick_timer_service_unsubscribe();
  if (s_refresh_timer) {
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void departures_window_push(const char *stop_id, const char *stop_name,
                            uint8_t type, int32_t color, int32_t textcol,
                            const char *badge) {
  strncpy(s_stop_id, stop_id, sizeof(s_stop_id) - 1);
  s_stop_id[sizeof(s_stop_id) - 1] = '\0';
  strncpy(s_stop_name, stop_name, sizeof(s_stop_name) - 1);
  s_stop_name[sizeof(s_stop_name) - 1] = '\0';
  memset(s_stop_badge, 0, sizeof(s_stop_badge));
  if (badge) {
    strncpy(s_stop_badge, badge, sizeof(s_stop_badge) - 1);
  }
  s_stop_type = type;
  s_stop_color = color;
  s_stop_textcol = textcol;

  s_count = 0;
  s_loading = true;
  s_error = 0;

  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = prv_window_load,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
      .unload = prv_window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void departures_window_handle_departures(const CommItem *items, int count,
                                         int refresh_secs) {
  if (!s_window) {
    return;
  }
  if (count > COMM_MAX_ITEMS) {
    count = COMM_MAX_ITEMS;
  }
  memcpy(s_deps, items, sizeof(CommItem) * count);
  s_count = count;
  s_loading = false;
  s_error = 0;
  s_refresh_secs = refresh_secs;
  marquee_reset();
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
  prv_schedule_refresh();
}

void departures_window_handle_error(int code) {
  if (!s_window) {
    return;
  }
  s_loading = false;
  if (s_count == 0) {
    s_error = code; // otherwise keep showing the last good list
  }
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
  prv_schedule_refresh();
}

bool departures_window_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

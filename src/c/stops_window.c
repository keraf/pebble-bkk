#include "stops_window.h"

#include <stdio.h>
#include <string.h>

#include "departures_window.h"
#include "favorites.h"
#include "locale.h"
#include "marquee.h"
#include "transit_types.h"

#define CELL_HEIGHT 44

enum {
  SectionFavorites = 0,
  SectionNearby = 1,
  SectionCount = 2,
};

static Window *s_window;
static MenuLayer *s_menu;

static CommItem s_nearby[COMM_MAX_ITEMS];
static int s_nearby_count;
static bool s_loading = true;
static int s_error;

static void prv_request(void) {
  s_loading = true;
  s_error = 0;
  comm_request_nearby();
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

static void prv_format_distance(int meters, char *buf, size_t len) {
  if (meters >= 1000) {
    snprintf(buf, len, "%d.%d km", meters / 1000, (meters % 1000) / 100);
  } else {
    snprintf(buf, len, "%d m", meters);
  }
}

static GColor prv_highlight_bg(void) {
  return PBL_IF_COLOR_ELSE(GColorFromHEX(0x522E91), GColorBlack);
}

static void prv_draw_item_cell(GContext *ctx, const Layer *cell_layer,
                               const char *badge_label, uint8_t type,
                               int32_t color, int32_t textcol,
                               const char *name, const char *subtitle) {
  GRect bounds = layer_get_bounds((Layer *)cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted((Layer *)cell_layer);

  GRect badge = GRect(4, (bounds.size.h - 28) / 2, 28, 28);
  int text_x = badge.origin.x + badge.size.w + 6;
  int text_w = bounds.size.w - text_x - 4;

  // Name first (may scroll past its area when selected), then cover the
  // strip left of the text and draw the badge on top of it.
  marquee_draw_text(ctx, GRect(text_x, 3, text_w, 24), name,
                    FONT_KEY_GOTHIC_18_BOLD, highlighted,
                    highlighted ? GColorWhite : GColorBlack);
  if (subtitle) {
    graphics_context_set_text_color(ctx, highlighted ? GColorWhite : GColorBlack);
    graphics_draw_text(ctx, subtitle, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(text_x, 24, text_w, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  if (highlighted) {
    graphics_context_set_fill_color(ctx, prv_highlight_bg());
    graphics_fill_rect(ctx, GRect(0, 0, text_x, bounds.size.h), 0, GCornerNone);
  }
  transit_draw_badge(ctx, badge, badge_label, type, color, textcol, highlighted);
}

static uint16_t prv_get_num_sections(MenuLayer *menu_layer, void *context) {
  return SectionCount;
}

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                                 void *context) {
  if (section_index == SectionFavorites) {
    return favorites_count();
  }
  if (s_loading || s_error) {
    return 1;
  }
  return s_nearby_count + 1; // + refresh row
}

static int16_t prv_get_header_height(MenuLayer *menu_layer,
                                     uint16_t section_index, void *context) {
  if (section_index == SectionFavorites && favorites_count() == 0) {
    return 0;
  }
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static int16_t prv_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index,
                                   void *context) {
  return CELL_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer,
                            uint16_t section_index, void *context) {
  if (section_index == SectionFavorites && favorites_count() == 0) {
    return;
  }
  transit_draw_menu_header(ctx, cell_layer,
      tr(section_index == SectionFavorites ? StrFavourites : StrNearbyStops));
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *cell_index, void *context) {
  if (cell_index->section == SectionFavorites) {
    const Favorite *fav = favorites_get(cell_index->row);
    if (!fav) {
      return;
    }
    char letter[2] = { transit_type_letter(fav->type), '\0' };
    prv_draw_item_cell(ctx, cell_layer, fav->badge[0] ? fav->badge : letter,
                       fav->type, fav->color, fav->textcol, fav->name,
                       tr(StrFavourite));
    return;
  }

  if (s_loading) {
    menu_cell_basic_draw(ctx, cell_layer, tr(StrLoading), tr(StrFindingStops), NULL);
    return;
  }
  if (s_error) {
    const char *title;
    const char *subtitle;
    comm_error_strings(s_error, &title, &subtitle);
    menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
    return;
  }
  if (cell_index->row >= s_nearby_count) {
    menu_cell_basic_draw(ctx, cell_layer, tr(StrRefresh), tr(StrUpdateNearby), NULL);
    return;
  }

  const CommItem *item = &s_nearby[cell_index->row];
  char letter[2] = { transit_type_letter(item->type), '\0' };
  char dist[16];
  prv_format_distance((int)item->extra, dist, sizeof(dist));
  prv_draw_item_cell(ctx, cell_layer, item->badge[0] ? item->badge : letter,
                     item->type, item->color, item->textcol, item->name, dist);
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index,
                             void *context) {
  if (cell_index->section == SectionFavorites) {
    const Favorite *fav = favorites_get(cell_index->row);
    if (fav) {
      departures_window_push(fav->id, fav->name, fav->type, fav->color,
                             fav->textcol, fav->badge);
    }
    return;
  }

  if (s_loading) {
    return;
  }
  if (s_error || cell_index->row >= s_nearby_count) {
    prv_request();
    return;
  }
  const CommItem *item = &s_nearby[cell_index->row];
  departures_window_push(item->sub, item->name, item->type, item->color,
                         item->textcol, item->badge);
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
  // Favourites may have been toggled from the departures window.
  menu_layer_reload_data(s_menu);
}

static void prv_window_disappear(Window *window) {
  marquee_stop();
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

void stops_window_push(void) {
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
  if (comm_js_ready()) {
    prv_request();
  }
}

void stops_window_handle_stops(const CommItem *items, int count) {
  if (count > COMM_MAX_ITEMS) {
    count = COMM_MAX_ITEMS;
  }
  memcpy(s_nearby, items, sizeof(CommItem) * count);
  s_nearby_count = count;
  s_loading = false;
  s_error = 0;
  marquee_reset();
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

void stops_window_handle_error(int code) {
  if (s_nearby_count > 0) {
    return; // keep showing the stale-but-useful list
  }
  s_loading = false;
  s_error = code;
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}

void stops_window_handle_ready(void) {
  // JS became ready, or settings changed: redraw (language may have changed)
  // and (re)try the fetch if we have nothing yet.
  if (!s_window) {
    return;
  }
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
  if (s_nearby_count == 0) {
    prv_request();
  }
}

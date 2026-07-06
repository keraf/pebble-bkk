#include "transit_types.h"

#include <string.h>

char transit_type_letter(uint8_t type) {
  switch (type) {
    case TransitTypeSubway: return 'M';
    case TransitTypeRail: return 'H';
    case TransitTypeTram: return 'T';
    case TransitTypeTrolleybus: return 'O';
    case TransitTypeBus: return 'B';
    case TransitTypeFerry: return 'F';
    default: return '?';
  }
}

GColor transit_badge_color(uint8_t type, int32_t api_color) {
#if defined(PBL_COLOR)
  if (api_color >= 0) {
    return GColorFromHEX((uint32_t)api_color);
  }
  // Fallback palette approximating BKK signage when the API omits a colour.
  switch (type) {
    case TransitTypeSubway: return GColorBlue;
    case TransitTypeRail: return GColorIslamicGreen;
    case TransitTypeTram: return GColorChromeYellow;
    case TransitTypeTrolleybus: return GColorRed;
    case TransitTypeBus: return GColorBlueMoon;
    case TransitTypeFerry: return GColorVividCerulean;
    default: return GColorDarkGray;
  }
#else
  (void)type;
  (void)api_color;
  return GColorBlack;
#endif
}

GColor transit_badge_text_color(int32_t api_textcol) {
#if defined(PBL_COLOR)
  if (api_textcol >= 0) {
    return GColorFromHEX((uint32_t)api_textcol);
  }
#else
  (void)api_textcol;
#endif
  return GColorWhite;
}

void transit_draw_badge(GContext *ctx, GRect box, const char *label,
                        uint8_t type, int32_t api_color, int32_t api_textcol,
                        bool highlighted) {
  GColor bg;
  GColor fg;
#if defined(PBL_COLOR)
  (void)highlighted;
  bg = transit_badge_color(type, api_color);
  fg = transit_badge_text_color(api_textcol);
#else
  // Monochrome: invert against the (possibly highlighted) row background.
  bg = highlighted ? GColorWhite : GColorBlack;
  fg = highlighted ? GColorBlack : GColorWhite;
  (void)type;
  (void)api_color;
  (void)api_textcol;
#endif

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, box, 4, GCornersAll);

  bool small = strlen(label) > 2;
  const char *font_key = small ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_18_BOLD;
  GRect text_box = box;
  text_box.origin.y += small ? 5 : 2;
  text_box.size.h -= small ? 5 : 2;

  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, label, fonts_get_system_font(font_key), text_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void transit_draw_menu_header(GContext *ctx, const Layer *cell_layer,
                              const char *text) {
#if defined(PBL_ROUND)
  GRect bounds = layer_get_bounds((Layer *)cell_layer);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, -3, bounds.size.w, bounds.size.h + 3),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  menu_cell_basic_header_draw(ctx, cell_layer, text);
#endif
}

#pragma once

#include <pebble.h>

// Wire codes shared with src/pkjs/index.js (TYPE_CODES). Lower value = higher
// priority when a stop serves several modes.
typedef enum {
  TransitTypeSubway = 0,
  TransitTypeRail = 1,
  TransitTypeTram = 2,
  TransitTypeTrolleybus = 3,
  TransitTypeBus = 4,
  TransitTypeFerry = 5,
  TransitTypeOther = 6,
} TransitType;

char transit_type_letter(uint8_t type);
GColor transit_badge_color(uint8_t type, int32_t api_color);
GColor transit_badge_text_color(int32_t api_textcol);

// Draws a rounded, coloured badge with a centered label (type letter or route
// short name). Degrades to black/white on monochrome platforms.
void transit_draw_badge(GContext *ctx, GRect box, const char *label,
                        uint8_t type, int32_t api_color, int32_t api_textcol,
                        bool highlighted);

// Section header that stays readable on round displays (centered there,
// standard left-aligned header on rectangular screens).
void transit_draw_menu_header(GContext *ctx, const Layer *cell_layer,
                              const char *text);

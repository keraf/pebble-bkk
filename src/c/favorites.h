#pragma once

#include <pebble.h>

#define FAVORITES_MAX 6

typedef struct {
  char id[96]; // comma-joined stop ids (nearest 6 platforms)
  char name[28];
  char badge[8]; // line name (M2, H5...) for metro/HÉV stops, else empty
  uint8_t type;
  int32_t color;
  int32_t textcol;
} Favorite;

void favorites_load(void);
int favorites_count(void);
const Favorite *favorites_get(int index);
bool favorites_contains(const char *stop_id);

// Adds or removes the stop. Returns true if it is a favourite after the call.
bool favorites_toggle(const Favorite *fav);

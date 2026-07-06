#pragma once

#include <pebble.h>

#define COMM_MAX_ITEMS 8

// One list row received from the phone. For stop lists `sub` is the stop id
// and `extra` the distance in meters; for departures `sub` is the headsign,
// `time` the departure as epoch seconds and `extra` the realtime flag.
typedef struct {
  char name[32];
  char sub[96]; // stop: comma-joined stop ids (nearest 6 platforms); departure: headsign
  char badge[8]; // line name (M2, H5...) for metro/HÉV stops, else empty
  uint8_t type;
  int32_t time;
  int32_t extra;
  int32_t color;
  int32_t textcol;
} CommItem;

// Error codes, kept in sync with src/pkjs/index.js.
enum {
  CommErrorNoApiKey = 1,
  CommErrorLocation = 2,
  CommErrorNetwork = 3,
  CommErrorNoResults = 4,
};

void comm_init(void);
bool comm_js_ready(void);
void comm_request_nearby(void);
void comm_request_departures(const char *stop_id);

// Human-readable title/subtitle for an error code.
void comm_error_strings(int code, const char **title, const char **subtitle);

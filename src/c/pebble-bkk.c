#include <pebble.h>

#include "comm.h"
#include "favorites.h"
#include "locale.h"
#include "stops_window.h"

int main(void) {
  locale_init();
  favorites_load();
  comm_init();
  stops_window_push();
  app_event_loop();
  return 0;
}

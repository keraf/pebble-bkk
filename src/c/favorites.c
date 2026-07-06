#include "favorites.h"

#include <string.h>

enum {
  PersistKeyVersion = 99,
  PersistKeyCount = 100,
  PersistKeyFirst = 101,
};

// Bump when the Favorite struct layout changes; older entries are discarded
// instead of being misread through the new layout.
#define FAVORITES_VERSION 4

static Favorite s_favs[FAVORITES_MAX];
static int s_count;

void favorites_load(void) {
  int version = persist_exists(PersistKeyVersion) ? persist_read_int(PersistKeyVersion) : 1;
  if (version != FAVORITES_VERSION) {
    persist_write_int(PersistKeyVersion, FAVORITES_VERSION);
    persist_delete(PersistKeyCount);
    for (int i = 0; i < FAVORITES_MAX; i++) {
      persist_delete(PersistKeyFirst + i);
    }
    s_count = 0;
    return;
  }
  s_count = persist_exists(PersistKeyCount) ? persist_read_int(PersistKeyCount) : 0;
  if (s_count < 0) {
    s_count = 0;
  }
  if (s_count > FAVORITES_MAX) {
    s_count = FAVORITES_MAX;
  }
  for (int i = 0; i < s_count; i++) {
    // memset first: entries persisted by an older layout may be shorter than
    // the current struct (e.g. missing badge), leaving the tail zeroed.
    memset(&s_favs[i], 0, sizeof(Favorite));
    if (persist_exists(PersistKeyFirst + i)) {
      persist_read_data(PersistKeyFirst + i, &s_favs[i], sizeof(Favorite));
    }
  }
}

static void prv_save(void) {
  persist_write_int(PersistKeyCount, s_count);
  for (int i = 0; i < s_count; i++) {
    persist_write_data(PersistKeyFirst + i, &s_favs[i], sizeof(Favorite));
  }
  for (int i = s_count; i < FAVORITES_MAX; i++) {
    persist_delete(PersistKeyFirst + i);
  }
}

int favorites_count(void) {
  return s_count;
}

const Favorite *favorites_get(int index) {
  return (index >= 0 && index < s_count) ? &s_favs[index] : NULL;
}

static int prv_index_of(const char *stop_id) {
  for (int i = 0; i < s_count; i++) {
    if (strcmp(s_favs[i].id, stop_id) == 0) {
      return i;
    }
  }
  return -1;
}

bool favorites_contains(const char *stop_id) {
  return prv_index_of(stop_id) >= 0;
}

bool favorites_toggle(const Favorite *fav) {
  int idx = prv_index_of(fav->id);
  if (idx >= 0) {
    for (int i = idx; i < s_count - 1; i++) {
      s_favs[i] = s_favs[i + 1];
    }
    s_count--;
    prv_save();
    return false;
  }
  if (s_count >= FAVORITES_MAX) {
    // Full: keep the existing favourites rather than silently evicting one.
    return false;
  }
  s_favs[s_count++] = *fav;
  prv_save();
  return true;
}

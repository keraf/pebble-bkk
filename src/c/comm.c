#include "comm.h"

#include <stdlib.h>
#include <string.h>

#include "departures_window.h"
#include "locale.h"
#include "stops_window.h"

// Message kinds, kept in sync with src/pkjs/index.js.
enum {
  MsgKindStops = 0,
  MsgKindDepartures = 1,
  MsgKindError = 2,
  MsgKindReady = 3,
};

enum {
  RequestNearby = 0,
  RequestDepartures = 1,
};

static bool s_js_ready;
static CommItem s_items[COMM_MAX_ITEMS];

static int prv_tuple_int(DictionaryIterator *iter, uint32_t key, int fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) {
    return fallback;
  }
  // Clay sends select/input values as strings; our own JS sends ints.
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);
  }
  return (int)t->value->int32;
}

static int prv_read_items(DictionaryIterator *iter, int count) {
  if (count > COMM_MAX_ITEMS) {
    count = COMM_MAX_ITEMS;
  }
  for (int i = 0; i < count; i++) {
    CommItem *it = &s_items[i];
    memset(it, 0, sizeof(*it));
    Tuple *name = dict_find(iter, MESSAGE_KEY_ITEM_NAME + i);
    if (name && name->type == TUPLE_CSTRING) {
      strncpy(it->name, name->value->cstring, sizeof(it->name) - 1);
    }
    Tuple *sub = dict_find(iter, MESSAGE_KEY_ITEM_SUB + i);
    if (sub && sub->type == TUPLE_CSTRING) {
      strncpy(it->sub, sub->value->cstring, sizeof(it->sub) - 1);
    }
    Tuple *badge = dict_find(iter, MESSAGE_KEY_ITEM_BADGE + i);
    if (badge && badge->type == TUPLE_CSTRING) {
      strncpy(it->badge, badge->value->cstring, sizeof(it->badge) - 1);
    }
    it->type = prv_tuple_int(iter, MESSAGE_KEY_ITEM_TYPE + i, 6);
    it->time = prv_tuple_int(iter, MESSAGE_KEY_ITEM_TIME + i, 0);
    it->extra = prv_tuple_int(iter, MESSAGE_KEY_ITEM_RT + i, 0);
    it->color = prv_tuple_int(iter, MESSAGE_KEY_ITEM_COLOR + i, -1);
    it->textcol = prv_tuple_int(iter, MESSAGE_KEY_ITEM_TEXTCOL + i, -1);
  }
  return count;
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  s_js_ready = true;

  // Language rides along on every message; Clay sends it as the string
  // "en"/"hu", our own JS as 0/1.
  Tuple *lang_t = dict_find(iter, MESSAGE_KEY_LANG);
  if (lang_t) {
    int lang = (lang_t->type == TUPLE_CSTRING)
        ? ((strcmp(lang_t->value->cstring, "hu") == 0) ? LangHu : LangEn)
        : (int)lang_t->value->int32;
    locale_set_lang(lang);
  }

  Tuple *kind_t = dict_find(iter, MESSAGE_KEY_MSG_KIND);
  if (!kind_t) {
    // Settings pushed by Clay (API key / radius / refresh): give a stuck
    // stops window a chance to retry with the new settings.
    stops_window_handle_ready();
    return;
  }

  switch ((int)kind_t->value->int32) {
    case MsgKindReady:
      stops_window_handle_ready();
      break;
    case MsgKindStops: {
      int count = prv_read_items(iter, prv_tuple_int(iter, MESSAGE_KEY_COUNT, 0));
      stops_window_handle_stops(s_items, count);
      break;
    }
    case MsgKindDepartures: {
      int count = prv_read_items(iter, prv_tuple_int(iter, MESSAGE_KEY_COUNT, 0));
      int refresh = prv_tuple_int(iter, MESSAGE_KEY_REFRESH_SECS, 60);
      departures_window_handle_departures(s_items, count, refresh);
      break;
    }
    case MsgKindError: {
      int code = prv_tuple_int(iter, MESSAGE_KEY_ERROR_CODE, CommErrorNetwork);
      if (departures_window_is_top()) {
        departures_window_handle_error(code);
      } else {
        stops_window_handle_error(code);
      }
      break;
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                              void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d", (int)reason);
}

static void prv_send_request(int request, const char *stop_id) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    return;
  }
  dict_write_int32(out, MESSAGE_KEY_REQUEST, request);
  if (stop_id) {
    dict_write_cstring(out, MESSAGE_KEY_STOP_ID, stop_id);
  }
  app_message_outbox_send();
}

void comm_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(2048, 128);
}

bool comm_js_ready(void) {
  return s_js_ready;
}

void comm_request_nearby(void) {
  prv_send_request(RequestNearby, NULL);
}

void comm_request_departures(const char *stop_id) {
  prv_send_request(RequestDepartures, stop_id);
}

void comm_error_strings(int code, const char **title, const char **subtitle) {
  switch (code) {
    case CommErrorNoApiKey:
      *title = tr(StrErrNoKeyTitle);
      *subtitle = tr(StrErrNoKeySub);
      break;
    case CommErrorLocation:
      *title = tr(StrErrLocTitle);
      *subtitle = tr(StrErrLocSub);
      break;
    case CommErrorNoResults:
      *title = tr(StrErrEmptyTitle);
      *subtitle = tr(StrErrEmptySub);
      break;
    default:
      *title = tr(StrErrNetTitle);
      *subtitle = tr(StrErrNetSub);
      break;
  }
}

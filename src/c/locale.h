#pragma once

#include <pebble.h>

typedef enum {
  LangEn = 0,
  LangHu = 1,
} Lang;

typedef enum {
  StrFavourites = 0,
  StrNearbyStops,
  StrLoading,
  StrFindingStops,
  StrRefresh,
  StrUpdateNearby,
  StrFavourite,
  StrErrNoKeyTitle,
  StrErrNoKeySub,
  StrErrLocTitle,
  StrErrLocSub,
  StrErrEmptyTitle,
  StrErrEmptySub,
  StrErrNetTitle,
  StrErrNetSub,
  StrFetchingDepartures,
  StrEtaNow,
  StrEtaSecondsFmt,   // takes: tilde prefix (%s), seconds (%d)
  StrEtaOneMinuteFmt, // takes: tilde prefix (%s)
  StrEtaMinutesFmt,   // takes: tilde prefix (%s), minutes (%d)
  StrEtaOverHour,
  StrCount,
} StringId;

void locale_init(void);            // load the persisted language
bool locale_set_lang(int lang);    // returns true if it changed; persists
const char *tr(StringId id);

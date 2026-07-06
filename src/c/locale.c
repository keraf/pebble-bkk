#include "locale.h"

enum { PersistKeyLang = 50 };

static Lang s_lang = LangEn;

static const char *s_strings[2][StrCount] = {
  [LangEn] = {
    [StrFavourites] = "Favourites",
    [StrNearbyStops] = "Nearby stops",
    [StrLoading] = "Loading...",
    [StrFindingStops] = "Finding stops nearby",
    [StrRefresh] = "Refresh",
    [StrUpdateNearby] = "Update nearby stops",
    [StrFavourite] = "favourite",
    [StrErrNoKeyTitle] = "No API key",
    [StrErrNoKeySub] = "Set it in the Pebble phone app",
    [StrErrLocTitle] = "No location",
    [StrErrLocSub] = "Check phone GPS",
    [StrErrEmptyTitle] = "Nothing found",
    [StrErrEmptySub] = "Try a larger radius",
    [StrErrNetTitle] = "Network error",
    [StrErrNetSub] = "Press select to retry",
    [StrFetchingDepartures] = "Fetching departures",
    [StrEtaNow] = "Now",
    [StrEtaSecondsFmt] = "In %s%d seconds",
    [StrEtaOneMinuteFmt] = "In %s1 minute",
    [StrEtaMinutesFmt] = "In %s%d minutes",
    [StrEtaOverHour] = "In over an hour",
  },
  [LangHu] = {
    [StrFavourites] = "Kedvencek",
    [StrNearbyStops] = "Közeli megállók",
    [StrLoading] = "Betöltés...",
    [StrFindingStops] = "Megállók keresése",
    [StrRefresh] = "Frissítés",
    [StrUpdateNearby] = "Közeli megállók frissítése",
    [StrFavourite] = "kedvenc",
    [StrErrNoKeyTitle] = "Nincs API kulcs",
    [StrErrNoKeySub] = "Állítsd be a Pebble appban",
    [StrErrLocTitle] = "Nincs helyadat",
    [StrErrLocSub] = "Ellenőrizd a GPS-t",
    [StrErrEmptyTitle] = "Nincs találat",
    [StrErrEmptySub] = "Próbálj nagyobb sugarat",
    [StrErrNetTitle] = "Hálózati hiba",
    [StrErrNetSub] = "Select gomb: újra",
    [StrFetchingDepartures] = "Indulások lekérése",
    [StrEtaNow] = "Most",
    [StrEtaSecondsFmt] = "%s%d másodperc múlva",
    [StrEtaOneMinuteFmt] = "%s1 perc múlva",
    [StrEtaMinutesFmt] = "%s%d perc múlva",
    [StrEtaOverHour] = "Több mint 1 óra",
  },
};

void locale_init(void) {
  int lang = persist_exists(PersistKeyLang) ? persist_read_int(PersistKeyLang) : LangEn;
  s_lang = (lang == LangHu) ? LangHu : LangEn;
}

bool locale_set_lang(int lang) {
  Lang new_lang = (lang == LangHu) ? LangHu : LangEn;
  if (new_lang == s_lang) {
    return false;
  }
  s_lang = new_lang;
  persist_write_int(PersistKeyLang, s_lang);
  return true;
}

const char *tr(StringId id) {
  if (id >= StrCount) {
    return "";
  }
  const char *s = s_strings[s_lang][id];
  return s ? s : "";
}

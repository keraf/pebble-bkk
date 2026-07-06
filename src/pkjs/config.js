// Clay settings page, localized. Exported as a function: index.js picks the
// language from the saved settings at startup, so a language change shows up
// on the settings page the next time the app starts.
var STRINGS = {
  en: {
    intro: "Nearby BKK stops and live departures on your Pebble.",
    apiSection: "BKK API",
    apiKey: "API key",
    apiPlaceholder: "Paste your BKK API key",
    apiHelp:
      'Get a free key at <a href="https://opendata.bkk.hu">opendata.bkk.hu</a>: register, then create a key for the FUTÁR API.',
    searchSection: "Search",
    radius: "Stop search radius",
    km: "1 km",
    refresh: "Auto-refresh departures",
    manual: "Manual only",
    every30: "Every 30 s",
    every60: "Every 60 s",
    language: "Language / Nyelv",
    save: "Save"
  },
  hu: {
    intro: "Közeli BKK megállók és élő indulások a Pebble órádon.",
    apiSection: "BKK API",
    apiKey: "API kulcs",
    apiPlaceholder: "Illeszd be a BKK API kulcsod",
    apiHelp:
      'Ingyenes kulcs: <a href="https://opendata.bkk.hu">opendata.bkk.hu</a> — regisztrálj, majd igényelj FUTÁR API kulcsot.',
    searchSection: "Keresés",
    radius: "Megállókeresési sugár",
    km: "1 km",
    refresh: "Indulások automatikus frissítése",
    manual: "Csak kézzel",
    every30: "30 mp-enként",
    every60: "60 mp-enként",
    language: "Nyelv / Language",
    save: "Mentés"
  }
};

module.exports = function (lang) {
  var t = STRINGS[lang] || STRINGS.en;
  return [
    {
      type: "heading",
      defaultValue: "BKK Departures"
    },
    {
      type: "text",
      defaultValue: t.intro
    },
    {
      type: "section",
      items: [
        {
          type: "heading",
          defaultValue: t.apiSection
        },
        {
          type: "input",
          messageKey: "API_KEY",
          label: t.apiKey,
          defaultValue: "",
          attributes: {
            placeholder: t.apiPlaceholder,
            type: "text"
          }
        },
        {
          type: "text",
          defaultValue: t.apiHelp
        }
      ]
    },
    {
      type: "section",
      items: [
        {
          type: "heading",
          defaultValue: t.searchSection
        },
        {
          type: "select",
          messageKey: "RADIUS",
          label: t.radius,
          defaultValue: "400",
          options: [
            { label: "200 m", value: "200" },
            { label: "400 m", value: "400" },
            { label: "600 m", value: "600" },
            { label: t.km, value: "1000" }
          ]
        },
        {
          type: "select",
          messageKey: "REFRESH_SECS",
          label: t.refresh,
          defaultValue: "60",
          options: [
            { label: t.manual, value: "0" },
            { label: t.every30, value: "30" },
            { label: t.every60, value: "60" }
          ]
        },
        {
          type: "select",
          messageKey: "LANG",
          label: t.language,
          defaultValue: "en",
          options: [
            { label: "English", value: "en" },
            { label: "Magyar", value: "hu" }
          ]
        }
      ]
    },
    {
      type: "submit",
      defaultValue: t.save
    }
  ];
};

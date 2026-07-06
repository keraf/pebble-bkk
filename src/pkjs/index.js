/* BKK Departures — phone side.
 * All BKK API traffic funnels through enqueue()/run() below: one request in
 * flight, per-request dedupe window, and cached nearby-stop results, so the
 * user's API key never gets hammered no matter what the watch asks for.
 */

// Vendored Clay 1.0.4 bundle (the npm pebble package predates the flint and
// gabbro platforms and refuses to build for them; the watch-side part of the
// package is an empty stub anyway).
var Clay = require("./vendor/pebble-clay");
var buildConfig = require("./config");
var keys = require("message_keys");

function currentLang() {
  try {
    var s = JSON.parse(localStorage.getItem("clay-settings")) || {};
    return s.LANG === "hu" ? "hu" : "en";
  } catch (e) {
    return "en";
  }
}

var clay = new Clay(buildConfig(currentLang()));

var BASE_URL = "https://futar.bkk.hu/api/query/v1/ws/otp/api/where/";

// Set to e.g. { lat: 47.4979, lon: 19.0546 } (Deák Ferenc tér) when testing in
// the emulator — its fake GPS reports a US location and BKK only covers Budapest.
var DEBUG_LOCATION = null;

var MAX_ITEMS = 8;
var MIN_GAP_MS = 20 * 1000; // dedupe window for identical requests
var STOPS_TTL_MS = 120 * 1000; // nearby-stops cache lifetime
var XHR_TIMEOUT_MS = 15 * 1000;

var REQ_NEARBY = 0;
var REQ_DEPARTURES = 1;

var KIND_STOPS = 0;
var KIND_DEPARTURES = 1;
var KIND_ERROR = 2;
var KIND_READY = 3;

var ERR_NO_KEY = 1;
var ERR_LOCATION = 2;
var ERR_NETWORK = 3;
var ERR_NO_RESULTS = 4;

// Priority doubles as the wire code: lower = higher priority when a stop
// serves several modes. Must match TransitType in src/c/transit_types.h.
var TYPE_CODES = {
  SUBWAY: 0,
  RAIL: 1,
  SUBURBAN_RAILWAY: 1,
  TRAM: 2,
  TROLLEYBUS: 3,
  BUS: 4,
  COACH: 4,
  FERRY: 5
};

function typeCode(t) {
  return TYPE_CODES[t] !== undefined ? TYPE_CODES[t] : 6;
}

// Fixed badge colours for the metro and HÉV lines (c = background,
// t = text). These override whatever the API reports so the badges always
// match the official line colours.
var LINE_STYLE = {
  M1: { c: 0xffd800, t: 0x000000 }, // yellow
  M2: { c: 0xe41f18, t: 0xffffff }, // red
  M3: { c: 0x005ca5, t: 0xffffff }, // dark blue
  M4: { c: 0x19a949, t: 0xffffff }, // green
  H5: { c: 0x821066, t: 0xffffff }, // purple
  H6: { c: 0xaa5500, t: 0xffffff }, // brown
  H7: { c: 0xee7203, t: 0xffffff }, // orange
  H8: { c: 0xffaaaa, t: 0x000000 }, // light pink
  H9: { c: 0xffaaaa, t: 0x000000 } // light pink
};

// Metro/HÉV stops and departures are badged with the line name (M2, H5...)
// in the line colour; everything else keeps the API colour / type letter.
function lineStyle(route) {
  if (!route || typeCode(route.type) > 1) return null; // SUBWAY/RAIL only
  return (route.shortName && LINE_STYLE[route.shortName]) || null;
}

function settings() {
  try {
    return JSON.parse(localStorage.getItem("clay-settings")) || {};
  } catch (e) {
    return {};
  }
}

function colorInt(hex) {
  if (!hex) return -1;
  var v = parseInt(String(hex).replace("#", ""), 16);
  return isNaN(v) ? -1 : v;
}

function truncBytes(str, maxBytes) {
  str = String(str || "");
  var out = "";
  var bytes = 0;
  for (var i = 0; i < str.length; i++) {
    var c = str.charCodeAt(i);
    var b = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
    if (bytes + b > maxBytes) break;
    out += str.charAt(i);
    bytes += b;
  }
  return out;
}

function distMeters(lat1, lon1, lat2, lon2) {
  var R = 6371000;
  var dLat = ((lat2 - lat1) * Math.PI) / 180;
  var dLon = ((lon2 - lon1) * Math.PI) / 180;
  var a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos((lat1 * Math.PI) / 180) *
      Math.cos((lat2 * Math.PI) / 180) *
      Math.sin(dLon / 2) *
      Math.sin(dLon / 2);
  return Math.round(R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a)));
}

function geolocate(cb) {
  if (DEBUG_LOCATION) {
    cb(null, DEBUG_LOCATION);
    return;
  }
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      cb(null, { lat: pos.coords.latitude, lon: pos.coords.longitude });
    },
    function (err) {
      console.log("geolocation error: " + JSON.stringify(err));
      cb(err || new Error("location"));
    },
    { enableHighAccuracy: true, timeout: 10000, maximumAge: 30000 }
  );
}

function buildUrl(path, params, apiKey) {
  var qs = [];
  for (var k in params) {
    if (!params.hasOwnProperty(k)) continue;
    var v = params[k];
    if (Object.prototype.toString.call(v) === "[object Array]") {
      for (var vi = 0; vi < v.length; vi++)
        qs.push(k + "=" + encodeURIComponent(v[vi]));
    } else {
      qs.push(k + "=" + encodeURIComponent(v));
    }
  }
  qs.push("key=" + encodeURIComponent(apiKey));
  qs.push("appVersion=pebble-bkk-1.0");
  return BASE_URL + path + "?" + qs.join("&");
}

function fetchJson(url, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.timeout = XHR_TIMEOUT_MS;
  xhr.onload = function () {
    if (xhr.status !== 200) {
      console.log("BKK HTTP " + xhr.status);
      cb(new Error("http " + xhr.status));
      return;
    }
    var body;
    try {
      body = JSON.parse(xhr.responseText);
    } catch (e) {
      cb(new Error("bad json"));
      return;
    }
    if (!body || body.status !== "OK" || !body.data) {
      console.log("BKK API status: " + (body && body.status));
      cb(new Error("api " + (body && body.status)));
      return;
    }
    cb(null, body);
  };
  xhr.onerror = function () {
    cb(new Error("network"));
  };
  xhr.ontimeout = function () {
    cb(new Error("timeout"));
  };
  xhr.send();
}

function sendDict(dict, cb) {
  Pebble.sendAppMessage(
    dict,
    function () {
      cb && cb();
    },
    function (e) {
      console.log("sendAppMessage failed, retrying once");
      Pebble.sendAppMessage(
        dict,
        function () {
          cb && cb();
        },
        function () {
          cb && cb();
        }
      );
    }
  );
}

function errorDict(code) {
  var d = {};
  d[keys.MSG_KIND] = KIND_ERROR;
  d[keys.ERROR_CODE] = code;
  d[keys.LANG] = currentLang() === "hu" ? 1 : 0;
  return d;
}

/* ---- request chokepoint ---- */

var inFlight = false;
var pending = null;
var cache = {}; // cacheKey -> { t: ms, dict: payload, ttl: ms }

function enqueue(req) {
  if (inFlight) {
    pending = req; // coalesce: only the latest queued request survives
    return;
  }
  run(req);
}

function done() {
  inFlight = false;
  if (pending) {
    var next = pending;
    pending = null;
    enqueue(next);
  }
}

function cachePut(cacheKey, dict, ttl) {
  cache[cacheKey] = { t: Date.now(), dict: dict, ttl: ttl };
}

// Errors are cached too (with a short TTL), so a failing key or a BKK outage
// can never be turned into a request storm by mashing refresh on the watch.
function cacheError(cacheKey, code) {
  var d = errorDict(code);
  cachePut(cacheKey, d, MIN_GAP_MS);
  return d;
}

function serveCached(cacheKey) {
  var c = cache[cacheKey];
  if (c && Date.now() - c.t < c.ttl) {
    console.log("cache hit: " + cacheKey);
    sendDict(c.dict, done);
    return true;
  }
  return false;
}

function run(req) {
  inFlight = true;
  var apiKey = String(settings().API_KEY || "").trim();
  if (!apiKey) {
    sendDict(errorDict(ERR_NO_KEY), done);
    return;
  }
  if (req.kind === REQ_DEPARTURES && req.stopId) {
    runDepartures(apiKey, req.stopId);
  } else {
    runNearby(apiKey);
  }
}

function runNearby(apiKey) {
  geolocate(function (err, loc) {
    if (err) {
      sendDict(errorDict(ERR_LOCATION), done);
      return;
    }
    var radius = parseInt(settings().RADIUS, 10) || 400;
    var cacheKey =
      "nearby:" + radius + ":" + loc.lat.toFixed(3) + "," + loc.lon.toFixed(3);
    if (serveCached(cacheKey)) return;

    var url = buildUrl(
      "stops-for-location.json",
      {
        lat: loc.lat,
        lon: loc.lon,
        radius: radius,
        includeReferences: "routes"
      },
      apiKey
    );
    console.log("fetch: stops-for-location r=" + radius);

    fetchJson(url, function (err2, body) {
      if (err2) {
        sendDict(cacheError(cacheKey, ERR_NETWORK), done);
        return;
      }
      var list = body.data.list || [];
      var routes = (body.data.references && body.data.references.routes) || {};
      // BKK models each direction as its own stop entity. Group stops by
      // name so one row covers both directions; the departures request then
      // queries all the grouped stop ids at once.
      var groups = {};
      var stops = [];
      for (var i = 0; i < list.length; i++) {
        var s = list[i];
        if (s.locationType) continue; // only real boarding stops
        if (!s.routeIds || !s.routeIds.length) continue;
        var best = null;
        for (var j = 0; j < s.routeIds.length; j++) {
          var rt = routes[s.routeIds[j]];
          if (rt && (!best || typeCode(rt.type) < typeCode(best.type)))
            best = rt;
        }
        var dist = distMeters(loc.lat, loc.lon, s.lat, s.lon);
        var g = groups[s.name];
        if (!g) {
          g = groups[s.name] = {
            name: s.name,
            children: [],
            dist: dist,
            best: best
          };
          stops.push(g);
        }
        g.children.push({ id: s.id, dist: dist });
        if (dist < g.dist) g.dist = dist;
        if (best && (!g.best || typeCode(best.type) < typeCode(g.best.type)))
          g.best = best;
      }
      for (var m = 0; m < stops.length; m++) {
        var grp = stops[m];
        var style = lineStyle(grp.best);
        // Nearest platforms first; big hubs can have 10+ same-named children
        // and only the closest 6 fit in the id list.
        grp.children.sort(function (a, b) {
          return a.dist - b.dist;
        });
        grp.id = grp.children
          .slice(0, 6)
          .map(function (c) {
            return c.id;
          })
          .join(",");
        grp.type = grp.best ? typeCode(grp.best.type) : 6;
        grp.badge = style && grp.best.shortName ? grp.best.shortName : "";
        grp.color = style ? style.c : colorInt(grp.best && grp.best.color);
        grp.textcol = style
          ? style.t
          : colorInt(grp.best && grp.best.textColor);
      }
      stops.sort(function (a, b) {
        return a.dist - b.dist;
      });
      stops = stops.slice(0, MAX_ITEMS);

      if (!stops.length) {
        sendDict(cacheError(cacheKey, ERR_NO_RESULTS), done);
        return;
      }

      var d = {};
      d[keys.MSG_KIND] = KIND_STOPS;
      d[keys.COUNT] = stops.length;
      d[keys.LANG] = currentLang() === "hu" ? 1 : 0;
      for (var n = 0; n < stops.length; n++) {
        d[keys.ITEM_NAME + n] = truncBytes(stops[n].name, 28);
        d[keys.ITEM_SUB + n] = truncBytes(stops[n].id, 92);
        d[keys.ITEM_TYPE + n] = stops[n].type;
        d[keys.ITEM_TIME + n] = 0;
        d[keys.ITEM_RT + n] = stops[n].dist;
        d[keys.ITEM_COLOR + n] = stops[n].color;
        d[keys.ITEM_TEXTCOL + n] = stops[n].textcol;
        if (stops[n].badge) d[keys.ITEM_BADGE + n] = stops[n].badge;
      }
      cachePut(cacheKey, d, STOPS_TTL_MS);
      sendDict(d, done);
    });
  });
}

function runDepartures(apiKey, stopId) {
  var cacheKey = "dep:" + stopId;
  if (serveCached(cacheKey)) return;

  var url = buildUrl(
    "arrivals-and-departures-for-stop.json",
    {
      stopId: stopId.split(","), // one id per direction, repeated params
      minutesBefore: 0,
      minutesAfter: 40,
      limit: 16,
      includeReferences: "trips,routes"
    },
    apiKey
  );
  console.log("fetch: departures " + stopId);

  fetchJson(url, function (err, body) {
    if (err) {
      sendDict(cacheError(cacheKey, ERR_NETWORK), done);
      return;
    }
    // Single-stop queries answer with data.entry, multi-stop ones may answer
    // with data.list — merge the stopTimes of whatever we got.
    var entries = body.data.entry ? [body.data.entry] : body.data.list || [];
    var stopTimes = [];
    for (var ei = 0; ei < entries.length; ei++) {
      var sts = (entries[ei] && entries[ei].stopTimes) || [];
      for (var si = 0; si < sts.length; si++) stopTimes.push(sts[si]);
    }
    var refs = body.data.references || {};
    var trips = refs.trips || {};
    var routes = refs.routes || {};
    var now = Math.floor((body.currentTime || Date.now()) / 1000);

    var deps = [];
    for (var i = 0; i < stopTimes.length; i++) {
      var st = stopTimes[i];
      var t =
        st.predictedDepartureTime ||
        st.departureTime ||
        st.predictedArrivalTime ||
        st.arrivalTime;
      if (!t || t < now - 30) continue;
      var trip = trips[st.tripId] || {};
      var route = routes[trip.routeId] || {};
      var style = lineStyle(route);
      deps.push({
        name: route.shortName || "?",
        sub: st.stopHeadsign || trip.tripHeadsign || "",
        type: typeCode(route.type),
        time: t,
        rt: st.predictedDepartureTime || st.predictedArrivalTime ? 1 : 0,
        color: style ? style.c : colorInt(route.color),
        textcol: style ? style.t : colorInt(route.textColor)
      });
    }
    deps.sort(function (a, b) {
      return a.time - b.time;
    });
    deps = deps.slice(0, MAX_ITEMS);

    if (!deps.length) {
      sendDict(cacheError(cacheKey, ERR_NO_RESULTS), done);
      return;
    }

    var d = {};
    d[keys.MSG_KIND] = KIND_DEPARTURES;
    d[keys.COUNT] = deps.length;
    d[keys.LANG] = currentLang() === "hu" ? 1 : 0;
    d[keys.REFRESH_SECS] = parseInt(settings().REFRESH_SECS, 10);
    if (isNaN(d[keys.REFRESH_SECS])) d[keys.REFRESH_SECS] = 60;
    for (var n = 0; n < deps.length; n++) {
      d[keys.ITEM_NAME + n] = truncBytes(deps[n].name, 7);
      d[keys.ITEM_SUB + n] = truncBytes(deps[n].sub, 36);
      d[keys.ITEM_TYPE + n] = deps[n].type;
      d[keys.ITEM_TIME + n] = deps[n].time;
      d[keys.ITEM_RT + n] = deps[n].rt;
      d[keys.ITEM_COLOR + n] = deps[n].color;
      d[keys.ITEM_TEXTCOL + n] = deps[n].textcol;
    }
    cachePut(cacheKey, d, MIN_GAP_MS);
    sendDict(d, done);
  });
}

/* ---- events ---- */

Pebble.addEventListener("ready", function () {
  console.log("pkjs ready");
  var d = {};
  d[keys.MSG_KIND] = KIND_READY;
  sendDict(d, null);
});

Pebble.addEventListener("appmessage", function (e) {
  var p = e.payload;
  if (p.REQUEST === undefined || p.REQUEST === null) return;
  enqueue({
    kind: p.REQUEST,
    stopId: p.STOP_ID ? String(p.STOP_ID) : null
  });
});

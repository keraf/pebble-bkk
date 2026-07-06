# Store page copy

## Short description (one-liner)

Live Budapest public transport departures on your wrist — nearby BKK stops,
real-time countdowns, and favourites.

## Full description

BKK Departures shows you the next departures of Budapest public transport,
right on your Pebble.

Open the app and it finds the stops around you using your phone's GPS. Every
stop is tagged with its mode — metro, tram, bus, trolleybus, HÉV or ferry —
using the official BKK line colours (yellow M1, red M2, blue M3, green M4,
and the HÉV line colours) on colour watches. Stops serving both directions
are combined, so you see everything leaving from where you stand.

Pick a stop and you get the upcoming departures with a live countdown that
ticks every second on the watch: "In 3 minutes", "In 45 seconds", "Now".
Real-time predictions are marked green; schedule-only times are prefixed
with ~. The list refreshes automatically while you watch it (configurable:
30 s, 60 s, or manual only).

Long-press SELECT on a stop's departures to pin it as a favourite — it shows
at the top of the home screen and survives restarts.

Available in English and Hungarian (switchable in the settings).

You need a free BKK OpenData API key: register at opendata.bkk.hu, request a
FUTÁR API key, and paste it into the app's settings page in the Pebble phone
app. The app is deliberately gentle with the API — requests are throttled,
cached and deduplicated, so your key stays comfortably within the usage
limits.

Works on all Pebble models, round and rectangular, colour and black & white.

Data source: BKK FUTÁR open data (opendata.bkk.hu). This app is not
affiliated with BKK.

## Support / source

Source code: https://github.com/keraf/pebble-bkk

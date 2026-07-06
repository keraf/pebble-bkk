#pragma once

#include <pebble.h>

// Horizontal marquee for the selected menu row: waits a moment, scrolls the
// text to its end, holds, snaps back, repeats. One instance app-wide (only
// one row is ever selected on screen).

typedef void (*MarqueeDirtyCallback)(void);

// Install the redraw callback for the currently visible window (call on
// appear). Resets any running animation.
void marquee_set_callback(MarqueeDirtyCallback cb);

// Call when the menu selection moves or the list is reloaded.
void marquee_reset(void);

// Call on window disappear to cancel the timer.
void marquee_stop(void);

// Draws `text` in `area`. On the selected row (`selected` true), text wider
// than the area scrolls horizontally; otherwise it is ellipsized. Scrolled
// glyphs can extend a few pixels past `area` on either side — callers draw
// the badge / right column after this so those regions get covered.
void marquee_draw_text(GContext *ctx, GRect area, const char *text,
                       const char *font_key, bool selected, GColor color);

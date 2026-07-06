#pragma once

#include "comm.h"

void departures_window_push(const char *stop_id, const char *stop_name,
                            uint8_t type, int32_t color, int32_t textcol,
                            const char *badge);

// Called by comm.c when the phone answers.
void departures_window_handle_departures(const CommItem *items, int count,
                                         int refresh_secs);
void departures_window_handle_error(int code);
bool departures_window_is_top(void);

#pragma once

#include "comm.h"

void stops_window_push(void);

// Called by comm.c when the phone answers.
void stops_window_handle_stops(const CommItem *items, int count);
void stops_window_handle_error(int code);
void stops_window_handle_ready(void);

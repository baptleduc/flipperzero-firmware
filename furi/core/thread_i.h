#pragma once

#include "thread.h"

#define THREAD_NOTIFY_INDEX               (1)
#define FURI_EVENT_LOOP_FLAG_NOTIFY_INDEX (2)

/**
 * The bit to set in FURI_EVENT_LOOP_FLAG_NOTIFY_INDEX when a bit has been set
 * in THREAD_NOTIFY_INDEX.
 */
#define FURI_EVENT_LOOP_NOTIFY_FLAGS_BIT (1 << 23)

void furi_thread_init(void);

void furi_thread_scrub(void);

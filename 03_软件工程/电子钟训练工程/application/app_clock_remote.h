/*
 * Infrared remote command mapping for the clock application.
 * The current implementation targets a common NEC 21-key remote and maps decoded commands to clock actions.
 */
#ifndef APP_CLOCK_REMOTE_H
#define APP_CLOCK_REMOTE_H

#include "app_clock_internal.h"
#include "drv_ir_remote.h"

void AppClockRemote_HandleEvent(ClockContext_t *clock,
                                DrvIrRemoteEvent_t event,
                                uint8_t *ui_dirty,
                                uint8_t *settings_dirty);

#endif

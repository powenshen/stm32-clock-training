#ifndef APP_CLOCK_UI_H
#define APP_CLOCK_UI_H

#include "app_clock_internal.h"

void AppClockUi_Init(TouchUiState_t *touch_ui);
void AppClockUi_HandleTouch(ClockContext_t *clock, TouchUiState_t *touch_ui, uint8_t *ui_dirty);
void AppClockUi_Render(const ClockContext_t *clock,
                       const TouchUiState_t *touch_ui,
                       uint8_t *ui_dirty,
                       const DrvRtcTime_t *now);

#endif

#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include "app_clock_internal.h"
#include "drv_rtc.h"
#include "sim_debug_config.h"
#include "stm32f10x.h"

typedef struct
{
    DrvRtcTime_t now;
    DrvRtcDate_t date;
    DrvRtcTime_t alarm_time;
    DrvRtcTime_t edit_time;
    uint32_t systick_ms;
    uint32_t last_activity_tick;
    uint32_t alarm_start_tick;
    uint8_t view;
    uint8_t rtc_source;
    uint8_t alarm_enabled;
    uint8_t alarm_ringing;
    uint8_t mute_enabled;
    uint8_t blink_state;
    uint8_t led_mask;
    uint8_t buzzer_on;
    uint8_t lcd_on;
} AppClockDebugSnapshot_t;

void App_Clock_Init(void);
void App_Clock_Task(void);
void App_Clock_Task1ms(void);
void App_Clock_GetDebugSnapshot(AppClockDebugSnapshot_t *snapshot);

#if APP_CLOCK_SIM_ENABLED
ClockContext_t *App_Clock_GetSimContext(void);
void App_Clock_SimInjectTouch(AppTouchButtonId_t button_id, uint8_t is_hold);
#endif

#endif

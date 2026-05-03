/*
 * Shared application-only data structures.
 * Keep these types private to the clock application so external modules only see App_Clock_* entry points.
 */
#ifndef APP_CLOCK_INTERNAL_H
#define APP_CLOCK_INTERNAL_H

#include "drv_rtc.h"
#include "stm32f10x.h"

typedef enum
{
    APP_VIEW_RUN = 0,
    APP_VIEW_SET_TIME_HOUR,
    APP_VIEW_SET_TIME_MINUTE,
    APP_VIEW_SET_TIME_SECOND,
    APP_VIEW_SET_ALARM_HOUR,
    APP_VIEW_SET_ALARM_MINUTE
} AppView_t;

typedef enum
{
    APP_TOUCH_BUTTON_NONE = 0,
    APP_TOUCH_BUTTON_1,
    APP_TOUCH_BUTTON_2,
    APP_TOUCH_BUTTON_3,
    APP_TOUCH_BUTTON_4
} AppTouchButtonId_t;

typedef struct
{
    DrvRtcTime_t alarm_time;
    DrvRtcTime_t edit_time;
    AppView_t view;
    DrvRtcSource_t rtc_source;
    uint8_t alarm_enabled;
    uint8_t alarm_ringing;
    uint8_t mute_enabled;
    uint32_t last_activity_tick;
    uint32_t alarm_start_tick;
    uint32_t last_alarm_second;
    uint32_t last_blink_tick;
    uint8_t blink_state;
} ClockContext_t;

typedef struct
{
    uint8_t pressed;
    uint8_t press_action_sent;
    AppTouchButtonId_t active_button;
    uint32_t press_start_tick;
    uint32_t last_repeat_tick;
} TouchUiState_t;

#endif

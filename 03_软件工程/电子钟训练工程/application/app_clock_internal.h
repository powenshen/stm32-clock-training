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
    DrvRtcTime_t alarm_time;        /* 当前闹钟时间 */
    DrvRtcTime_t edit_time;         /* 编辑态下的临时时间副本 */
    AppView_t view;                 /* 当前应用视图 */
    DrvRtcSource_t rtc_source;      /* 当前 RTC 时间来源 */
    uint8_t alarm_enabled;          /* 闹钟使能标志 */
    uint8_t alarm_ringing;          /* 闹钟响铃标志 */
    uint8_t mute_enabled;           /* 静音使能标志 */
    uint32_t last_activity_tick;    /* 最近一次编辑活动的毫秒时间戳 */
    uint32_t alarm_start_tick;      /* 本次响铃开始时的毫秒时间戳 */
    uint32_t last_alarm_second;     /* 上次检查过的秒值，用于防止重复触发 */
    uint32_t last_blink_tick;       /* 上次切换指示灯闪烁状态的毫秒时间戳 */
    uint8_t blink_state;            /* 当前闪烁输出状态 */
} ClockContext_t;

typedef struct
{
    uint8_t pressed;                 /* 当前是否处于按压状态 */
    uint8_t press_action_sent;       /* 当前这次按压是否已经发送过动作 */
    AppTouchButtonId_t active_button;/* 当前按下的逻辑按钮编号 */
    uint32_t press_start_tick;       /* 当前按压开始时的毫秒时间戳 */
    uint32_t last_repeat_tick;       /* 上次发送长按连发动作的毫秒时间戳 */
} TouchUiState_t;

#endif

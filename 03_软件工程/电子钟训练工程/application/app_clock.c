#include "app_clock.h"

#include <stdio.h>

#include "bsp_led.h"
#include "drv_debug_uart.h"
#include "drv_key.h"
#include "drv_rtc.h"
#include "drv_systick.h"

#define APP_EDIT_TIMEOUT_MS          10000UL
#define APP_ALARM_RING_MS            30000UL
#define APP_ALARM_BLINK_PERIOD_MS    200UL

typedef enum
{
    APP_VIEW_RUN = 0,
    APP_VIEW_SET_TIME_HOUR,
    APP_VIEW_SET_TIME_MINUTE,
    APP_VIEW_SET_TIME_SECOND,
    APP_VIEW_SET_ALARM_HOUR,
    APP_VIEW_SET_ALARM_MINUTE
} AppView_t;

typedef struct
{
    DrvRtcTime_t alarm_time;
    DrvRtcTime_t edit_time;
    AppView_t view;
    uint8_t alarm_enabled;
    uint8_t alarm_ringing;
    uint32_t last_activity_tick;
    uint32_t alarm_start_tick;
    uint32_t last_alarm_second;
    uint32_t last_blink_tick;
    uint8_t blink_state;
} ClockContext_t;

static ClockContext_t g_clock =
{
    {6U, 30U, 0U},
    {12U, 0U, 0U},
    APP_VIEW_RUN,
    0U,
    0U,
    0U,
    0U,
    0xFFFFFFFFUL,
    0U,
    0U
};

static const char *App_Clock_ViewName(AppView_t view)
{
    switch (view)
    {
        case APP_VIEW_RUN:
            return "RUN";

        case APP_VIEW_SET_TIME_HOUR:
            return "SET_TIME_HOUR";

        case APP_VIEW_SET_TIME_MINUTE:
            return "SET_TIME_MINUTE";

        case APP_VIEW_SET_TIME_SECOND:
            return "SET_TIME_SECOND";

        case APP_VIEW_SET_ALARM_HOUR:
            return "SET_ALARM_HOUR";

        case APP_VIEW_SET_ALARM_MINUTE:
            return "SET_ALARM_MINUTE";

        default:
            return "UNKNOWN";
    }
}

static const char *App_Clock_RtcSourceName(DrvRtcSource_t source)
{
    switch (source)
    {
        case DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME:
            return "RTC-HW(DEFAULT)";

        case DRV_RTC_SOURCE_HARDWARE_BACKUP:
            return "RTC-HW(BACKUP)";

        case DRV_RTC_SOURCE_SOFTWARE:
        default:
            return "RTC-SOFT";
    }
}

static uint8_t App_Clock_IsEditView(AppView_t view)
{
    return (uint8_t)(view != APP_VIEW_RUN);
}

static uint32_t App_Clock_TimeToSeconds(const DrvRtcTime_t *time)
{
    return (((uint32_t)time->hour * 3600UL) +
            ((uint32_t)time->minute * 60UL) +
            (uint32_t)time->second);
}

static void App_Clock_PrintState(const char *reason)
{
    DrvRtcTime_t now;

    Drv_Rtc_GetTime(&now);

    printf("[%s] now=%02u:%02u:%02u view=%s alarm=%s %02u:%02u ring=%u edit=%02u:%02u:%02u\n",
           reason,
           now.hour,
           now.minute,
           now.second,
           App_Clock_ViewName(g_clock.view),
           (g_clock.alarm_enabled != 0U) ? "ON" : "OFF",
           g_clock.alarm_time.hour,
           g_clock.alarm_time.minute,
           g_clock.alarm_ringing,
           g_clock.edit_time.hour,
           g_clock.edit_time.minute,
           g_clock.edit_time.second);
}

static void App_Clock_ApplyLedState(void)
{
    uint8_t led_mask;

    if (g_clock.alarm_ringing != 0U)
    {
        if (Drv_Systick_IsElapsed(&g_clock.last_blink_tick, APP_ALARM_BLINK_PERIOD_MS) != 0U)
        {
            g_clock.blink_state = (uint8_t)!g_clock.blink_state;
        }

        led_mask = (g_clock.blink_state != 0U) ? BSP_LED_RED_MASK : 0U;
        BSP_LED_SetMask(led_mask);
        return;
    }

    g_clock.blink_state = 0U;
    g_clock.last_blink_tick = Drv_Systick_Millis();

    if ((g_clock.view == APP_VIEW_SET_TIME_HOUR) ||
        (g_clock.view == APP_VIEW_SET_TIME_MINUTE) ||
        (g_clock.view == APP_VIEW_SET_TIME_SECOND))
    {
        BSP_LED_SetMask(BSP_LED_BLUE_MASK);
        return;
    }

    if ((g_clock.view == APP_VIEW_SET_ALARM_HOUR) ||
        (g_clock.view == APP_VIEW_SET_ALARM_MINUTE))
    {
        BSP_LED_SetMask((uint8_t)(BSP_LED_GREEN_MASK | BSP_LED_BLUE_MASK));
        return;
    }

    led_mask = (g_clock.alarm_enabled != 0U) ? BSP_LED_GREEN_MASK : BSP_LED_BLUE_MASK;
    BSP_LED_SetMask(led_mask);
}

static void App_Clock_TouchActivity(void)
{
    g_clock.last_activity_tick = Drv_Systick_Millis();
}

static void App_Clock_EnterTimeEdit(void)
{
    Drv_Rtc_GetTime(&g_clock.edit_time);
    g_clock.view = APP_VIEW_SET_TIME_HOUR;
    App_Clock_TouchActivity();
    App_Clock_PrintState("enter-time-edit");
}

static void App_Clock_EnterAlarmEdit(void)
{
    g_clock.edit_time = g_clock.alarm_time;
    g_clock.view = APP_VIEW_SET_ALARM_HOUR;
    App_Clock_TouchActivity();
    App_Clock_PrintState("enter-alarm-edit");
}

static void App_Clock_CancelEdit(const char *reason)
{
    g_clock.view = APP_VIEW_RUN;
    App_Clock_PrintState(reason);
}

static void App_Clock_SaveTimeEdit(void)
{
    Drv_Rtc_SetTime(&g_clock.edit_time);
    g_clock.view = APP_VIEW_RUN;
    App_Clock_PrintState("save-time");
}

static void App_Clock_SaveAlarmEdit(void)
{
    g_clock.alarm_time = g_clock.edit_time;
    g_clock.alarm_time.second = 0U;
    g_clock.alarm_enabled = 1U;
    g_clock.view = APP_VIEW_RUN;
    App_Clock_PrintState("save-alarm");
}

static void App_Clock_AdjustEditValue(int8_t delta)
{
    uint8_t *value;
    uint8_t limit;

    value = 0;
    limit = 0U;

    switch (g_clock.view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_ALARM_HOUR:
            value = &g_clock.edit_time.hour;
            limit = 24U;
            break;

        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_ALARM_MINUTE:
            value = &g_clock.edit_time.minute;
            limit = 60U;
            break;

        case APP_VIEW_SET_TIME_SECOND:
            value = &g_clock.edit_time.second;
            limit = 60U;
            break;

        default:
            return;
    }

    if (delta > 0)
    {
        *value = (uint8_t)((*value + 1U) % limit);
    }
    else
    {
        *value = (uint8_t)((*value == 0U) ? (limit - 1U) : (*value - 1U));
    }

    if (g_clock.view == APP_VIEW_SET_ALARM_HOUR)
    {
        g_clock.edit_time.second = 0U;
    }

    if (g_clock.view == APP_VIEW_SET_ALARM_MINUTE)
    {
        g_clock.edit_time.second = 0U;
    }

    App_Clock_TouchActivity();
    App_Clock_PrintState((delta > 0) ? "adjust++" : "adjust--");
}

static void App_Clock_AdvanceEditView(void)
{
    switch (g_clock.view)
    {
        case APP_VIEW_SET_TIME_HOUR:
            g_clock.view = APP_VIEW_SET_TIME_MINUTE;
            break;

        case APP_VIEW_SET_TIME_MINUTE:
            g_clock.view = APP_VIEW_SET_TIME_SECOND;
            break;

        case APP_VIEW_SET_TIME_SECOND:
            App_Clock_SaveTimeEdit();
            return;

        case APP_VIEW_SET_ALARM_HOUR:
            g_clock.view = APP_VIEW_SET_ALARM_MINUTE;
            break;

        case APP_VIEW_SET_ALARM_MINUTE:
            App_Clock_SaveAlarmEdit();
            return;

        default:
            return;
    }

    App_Clock_TouchActivity();
    App_Clock_PrintState("next-field");
}

static void App_Clock_StopAlarm(const char *reason)
{
    if (g_clock.alarm_ringing == 0U)
    {
        return;
    }

    g_clock.alarm_ringing = 0U;
    App_Clock_PrintState(reason);
}

static void App_Clock_StartAlarm(void)
{
    g_clock.alarm_ringing = 1U;
    g_clock.alarm_start_tick = Drv_Systick_Millis();
    g_clock.last_blink_tick = g_clock.alarm_start_tick;
    g_clock.blink_state = 1U;
    App_Clock_PrintState("alarm-start");
}

static void App_Clock_HandleRunKey(const KeyEvent_t *event)
{
    if ((event->key_id == KEY_ID_KEY1) && (event->type == KEY_EVENT_TYPE_CLICK))
    {
        App_Clock_EnterTimeEdit();
        return;
    }

    if ((event->key_id == KEY_ID_KEY1) && (event->type == KEY_EVENT_TYPE_LONG_PRESS))
    {
        g_clock.alarm_enabled = (uint8_t)!g_clock.alarm_enabled;
        App_Clock_PrintState("alarm-toggle");
        return;
    }

    if ((event->key_id == KEY_ID_KEY2) && (event->type == KEY_EVENT_TYPE_LONG_PRESS))
    {
        App_Clock_EnterAlarmEdit();
        return;
    }

    if ((event->key_id == KEY_ID_KEY2) && (event->type == KEY_EVENT_TYPE_CLICK))
    {
        App_Clock_PrintState("run-click");
    }
}

static void App_Clock_HandleEditKey(const KeyEvent_t *event)
{
    if ((event->key_id == KEY_ID_KEY1) && (event->type == KEY_EVENT_TYPE_CLICK))
    {
        App_Clock_AdvanceEditView();
        return;
    }

    if ((event->key_id == KEY_ID_KEY1) && (event->type == KEY_EVENT_TYPE_LONG_PRESS))
    {
        App_Clock_CancelEdit("cancel-edit");
        return;
    }

    if ((event->key_id == KEY_ID_KEY2) && (event->type == KEY_EVENT_TYPE_CLICK))
    {
        App_Clock_AdjustEditValue(1);
        return;
    }

    if ((event->key_id == KEY_ID_KEY2) &&
        ((event->type == KEY_EVENT_TYPE_LONG_PRESS) || (event->type == KEY_EVENT_TYPE_REPEAT)))
    {
        App_Clock_AdjustEditValue(-1);
    }
}

static void App_Clock_HandleKeyEvent(KeyEvent_t event)
{
    if (event.type == KEY_EVENT_TYPE_NONE)
    {
        return;
    }

    if (g_clock.alarm_ringing != 0U)
    {
        App_Clock_StopAlarm("alarm-stop-key");
        return;
    }

    if (App_Clock_IsEditView(g_clock.view) != 0U)
    {
        App_Clock_HandleEditKey(&event);
    }
    else
    {
        App_Clock_HandleRunKey(&event);
    }
}

static void App_Clock_CheckEditTimeout(void)
{
    uint32_t now_tick;

    if (App_Clock_IsEditView(g_clock.view) == 0U)
    {
        return;
    }

    now_tick = Drv_Systick_Millis();
    if ((now_tick - g_clock.last_activity_tick) >= APP_EDIT_TIMEOUT_MS)
    {
        App_Clock_CancelEdit("edit-timeout");
    }
}

static void App_Clock_CheckAlarm(const DrvRtcTime_t *now)
{
    uint32_t now_seconds;

    if (g_clock.alarm_ringing != 0U)
    {
        if ((Drv_Systick_Millis() - g_clock.alarm_start_tick) >= APP_ALARM_RING_MS)
        {
            App_Clock_StopAlarm("alarm-timeout");
        }
        return;
    }

    if ((g_clock.alarm_enabled == 0U) || (App_Clock_IsEditView(g_clock.view) != 0U))
    {
        return;
    }

    now_seconds = App_Clock_TimeToSeconds(now);
    if (now_seconds == g_clock.last_alarm_second)
    {
        return;
    }

    g_clock.last_alarm_second = now_seconds;
    if ((now->hour == g_clock.alarm_time.hour) &&
        (now->minute == g_clock.alarm_time.minute) &&
        (now->second == 0U))
    {
        App_Clock_StartAlarm();
    }
}

void App_Clock_Init(void)
{
    DrvRtcTime_t default_time;
    DrvRtcSource_t rtc_source;

    BSP_LED_Init();
    Drv_Key_Init();
    Drv_DebugUart_Init();
    Drv_Systick_Init();

    default_time.hour = 12U;
    default_time.minute = 0U;
    default_time.second = 0U;

    rtc_source = Drv_Rtc_Init(&default_time);

    Drv_DebugUart_SendString("Clock project refactor ready.\n");
    printf("RTC source: %s\n", App_Clock_RtcSourceName(rtc_source));
    Drv_DebugUart_SendString("KEY1 click : enter time setting / next field\n");
    Drv_DebugUart_SendString("KEY1 long  : toggle alarm in RUN / cancel edit\n");
    Drv_DebugUart_SendString("KEY2 click : increment field / print state in RUN\n");
    Drv_DebugUart_SendString("KEY2 long  : enter alarm setting in RUN / decrement field\n");
    Drv_DebugUart_SendString("KEY2 hold  : repeat decrement in edit mode\n");
    App_Clock_PrintState("boot");
}

void App_Clock_Task1ms(void)
{
    Drv_Key_Task1ms();
    Drv_Rtc_Task1ms();
}

void App_Clock_Task(void)
{
    static uint32_t last_printed_second = 0xFFFFFFFFUL;
    DrvRtcTime_t now;
    uint32_t now_seconds;

    App_Clock_HandleKeyEvent(Drv_Key_GetEvent());
    App_Clock_CheckEditTimeout();

    Drv_Rtc_GetTime(&now);
    now_seconds = App_Clock_TimeToSeconds(&now);

    if ((g_clock.view == APP_VIEW_RUN) && (now_seconds != last_printed_second))
    {
        last_printed_second = now_seconds;
        App_Clock_PrintState("tick");
    }

    App_Clock_CheckAlarm(&now);
    App_Clock_ApplyLedState();
}

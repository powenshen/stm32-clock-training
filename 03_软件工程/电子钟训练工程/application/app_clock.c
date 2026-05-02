#include "app_clock.h"

#include <stdio.h>

#include "bsp_lcd.h"
#include "bsp_led.h"
#include "bsp_touch.h"
#include "drv_debug_uart.h"
#include "drv_key.h"
#include "drv_rtc.h"
#include "drv_systick.h"

#define APP_EDIT_TIMEOUT_MS          10000UL
#define APP_ALARM_RING_MS            30000UL
#define APP_ALARM_BLINK_PERIOD_MS    200UL
#define APP_TOUCH_REPEAT_START_MS    400UL
#define APP_TOUCH_REPEAT_PERIOD_MS   150UL

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

static ClockContext_t g_clock =
{
    {6U, 30U, 0U},
    {12U, 0U, 0U},
    APP_VIEW_RUN,
    DRV_RTC_SOURCE_SOFTWARE,
    0U,
    0U,
    0U,
    0U,
    0xFFFFFFFFUL,
    0U,
    0U
};

static TouchUiState_t g_touch_ui = {0U, 0U, APP_TOUCH_BUTTON_NONE, 0U, 0U};
static uint8_t g_ui_dirty = 1U;

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

static const char *App_Clock_RtcSourceLabel(DrvRtcSource_t source)
{
    switch (source)
    {
        case DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME:
            return "DEFAULT";

        case DRV_RTC_SOURCE_HARDWARE_BACKUP:
            return "BACKUP";

        case DRV_RTC_SOURCE_SOFTWARE:
        default:
            return "SOFT";
    }
}

static const char *App_Clock_ModeLabel(void)
{
    switch (g_clock.view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_TIME_SECOND:
            return "TIME SET";

        case APP_VIEW_SET_ALARM_HOUR:
        case APP_VIEW_SET_ALARM_MINUTE:
            return "ALARM SET";

        case APP_VIEW_RUN:
        default:
            if (g_clock.alarm_ringing != 0U)
            {
                return "RINGING";
            }
            return "RUN";
    }
}

static const char *App_Clock_EditFieldLabel(void)
{
    switch (g_clock.view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_ALARM_HOUR:
            return "HOUR";

        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_ALARM_MINUTE:
            return "MIN";

        case APP_VIEW_SET_TIME_SECOND:
            return "SEC";

        default:
            return "";
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

static void App_Clock_InvalidateUi(void)
{
    g_ui_dirty = 1U;
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
            App_Clock_InvalidateUi();
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
    App_Clock_InvalidateUi();
    App_Clock_PrintState("enter-time-edit");
}

static void App_Clock_EnterAlarmEdit(void)
{
    g_clock.edit_time = g_clock.alarm_time;
    g_clock.view = APP_VIEW_SET_ALARM_HOUR;
    App_Clock_TouchActivity();
    App_Clock_InvalidateUi();
    App_Clock_PrintState("enter-alarm-edit");
}

static void App_Clock_CancelEdit(const char *reason)
{
    g_clock.view = APP_VIEW_RUN;
    App_Clock_InvalidateUi();
    App_Clock_PrintState(reason);
}

static void App_Clock_SaveTimeEdit(void)
{
    Drv_Rtc_SetTime(&g_clock.edit_time);
    g_clock.view = APP_VIEW_RUN;
    App_Clock_InvalidateUi();
    App_Clock_PrintState("save-time");
}

static void App_Clock_SaveAlarmEdit(void)
{
    g_clock.alarm_time = g_clock.edit_time;
    g_clock.alarm_time.second = 0U;
    g_clock.alarm_enabled = 1U;
    g_clock.view = APP_VIEW_RUN;
    App_Clock_InvalidateUi();
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

    if ((g_clock.view == APP_VIEW_SET_ALARM_HOUR) || (g_clock.view == APP_VIEW_SET_ALARM_MINUTE))
    {
        g_clock.edit_time.second = 0U;
    }

    App_Clock_TouchActivity();
    App_Clock_InvalidateUi();
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
    App_Clock_InvalidateUi();
    App_Clock_PrintState("next-field");
}

static void App_Clock_StopAlarm(const char *reason)
{
    if (g_clock.alarm_ringing == 0U)
    {
        return;
    }

    g_clock.alarm_ringing = 0U;
    App_Clock_InvalidateUi();
    App_Clock_PrintState(reason);
}

static void App_Clock_StartAlarm(void)
{
    g_clock.alarm_ringing = 1U;
    g_clock.alarm_start_tick = Drv_Systick_Millis();
    g_clock.last_blink_tick = g_clock.alarm_start_tick;
    g_clock.blink_state = 1U;
    App_Clock_InvalidateUi();
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
        App_Clock_InvalidateUi();
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

static void App_Clock_GetButtonRect(AppTouchButtonId_t button_id,
                                    uint16_t *x,
                                    uint16_t *y,
                                    uint16_t *width,
                                    uint16_t *height)
{
    uint16_t screen_width;
    uint16_t screen_height;
    uint16_t margin;
    uint16_t gap;
    uint16_t button_width;
    uint16_t button_height;
    uint16_t base_x;

    screen_width = BSP_LCD_GetWidth();
    screen_height = BSP_LCD_GetHeight();
    margin = 8U;
    gap = 8U;
    button_height = 50U;
    button_width = (uint16_t)((screen_width - margin * 2U - gap * 3U) / 4U);
    base_x = (uint16_t)(margin + ((uint16_t)(button_id - 1U) * (button_width + gap)));

    *x = base_x;
    *y = (uint16_t)(screen_height - margin - button_height);
    *width = button_width;
    *height = button_height;
}

static AppTouchButtonId_t App_Clock_GetTouchButtonId(const BspTouchPoint_t *point)
{
    AppTouchButtonId_t button_id;

    if (point == 0)
    {
        return APP_TOUCH_BUTTON_NONE;
    }

    for (button_id = APP_TOUCH_BUTTON_1; button_id <= APP_TOUCH_BUTTON_4; button_id++)
    {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;

        App_Clock_GetButtonRect(button_id, &x, &y, &width, &height);
        if ((point->x >= x) && (point->x < (uint16_t)(x + width)) &&
            (point->y >= y) && (point->y < (uint16_t)(y + height)))
        {
            return button_id;
        }
    }

    return APP_TOUCH_BUTTON_NONE;
}

static void App_Clock_DrawTouchButton(AppTouchButtonId_t button_id,
                                      const char *label,
                                      uint16_t base_color,
                                      uint8_t pressed)
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t fill_color;

    App_Clock_GetButtonRect(button_id, &x, &y, &width, &height);
    fill_color = base_color;
    if (pressed != 0U)
    {
        fill_color = BSP_LCD_COLOR_WHITE;
    }

    BSP_LCD_FillRect(x, y, width, height, fill_color);
    BSP_LCD_DrawRect(x, y, width, height, pressed != 0U ? base_color : BSP_LCD_COLOR_WHITE);
    BSP_LCD_DrawStringCentered(x,
                               (uint16_t)(y + 16U),
                               width,
                               label,
                               pressed != 0U ? base_color : BSP_LCD_COLOR_WHITE,
                               fill_color,
                               2U);
}

static void App_Clock_Render(const DrvRtcTime_t *now)
{
    char time_string[16];
    char line_buffer[32];
    const DrvRtcTime_t *display_time;
    uint16_t screen_width;
    uint16_t time_bg_color;

    screen_width = BSP_LCD_GetWidth();
    display_time = (App_Clock_IsEditView(g_clock.view) != 0U) ? &g_clock.edit_time : now;

    sprintf(time_string, "%02u:%02u:%02u", display_time->hour, display_time->minute, display_time->second);

    BSP_LCD_FillScreen(BSP_LCD_COLOR_BLACK);

    BSP_LCD_FillRect(0U, 0U, screen_width, 28U, BSP_LCD_COLOR_NAVY);
    BSP_LCD_DrawStringCentered(0U, 7U, screen_width, "CLOCK", BSP_LCD_COLOR_WHITE, BSP_LCD_COLOR_NAVY, 2U);

    time_bg_color = (g_clock.alarm_ringing != 0U) ? BSP_LCD_COLOR_RED : BSP_LCD_COLOR_DARK_GRAY;
    BSP_LCD_FillRect(12U, 40U, (uint16_t)(screen_width - 24U), 56U, time_bg_color);
    BSP_LCD_DrawStringCentered(12U, 50U, (uint16_t)(screen_width - 24U), time_string, BSP_LCD_COLOR_WHITE, time_bg_color, 6U);

    sprintf(line_buffer, "MODE %s", App_Clock_ModeLabel());
    BSP_LCD_DrawString(16U, 112U, line_buffer, BSP_LCD_COLOR_CYAN, BSP_LCD_COLOR_BLACK, 2U);

    sprintf(line_buffer,
            "ALARM %s %02u:%02u",
            (g_clock.alarm_enabled != 0U) ? "ON" : "OFF",
            g_clock.alarm_time.hour,
            g_clock.alarm_time.minute);
    BSP_LCD_DrawString(16U, 136U, line_buffer, BSP_LCD_COLOR_YELLOW, BSP_LCD_COLOR_BLACK, 2U);

    sprintf(line_buffer, "RTC %s", App_Clock_RtcSourceLabel(g_clock.rtc_source));
    BSP_LCD_DrawString(16U, 160U, line_buffer, BSP_LCD_COLOR_GREEN, BSP_LCD_COLOR_BLACK, 2U);

    if (App_Clock_IsEditView(g_clock.view) != 0U)
    {
        sprintf(line_buffer, "FIELD %s", App_Clock_EditFieldLabel());
        BSP_LCD_DrawString(16U, 184U, line_buffer, BSP_LCD_COLOR_MAGENTA, BSP_LCD_COLOR_BLACK, 2U);
    }
    else if (g_clock.alarm_ringing != 0U)
    {
        BSP_LCD_DrawString(16U, 184U, "TOUCH STOP", BSP_LCD_COLOR_RED, BSP_LCD_COLOR_BLACK, 2U);
    }
    else
    {
        BSP_LCD_DrawString(16U, 184U, "PHYS KEY OK", BSP_LCD_COLOR_GRAY, BSP_LCD_COLOR_BLACK, 2U);
    }

    if (g_clock.alarm_ringing != 0U)
    {
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_1, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_1));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_2, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_2));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_3, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_3));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_4, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_4));
    }
    else if (App_Clock_IsEditView(g_clock.view) != 0U)
    {
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_1, "NEXT", BSP_LCD_COLOR_BLUE, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_1));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_2, "BACK", BSP_LCD_COLOR_ORANGE, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_2));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_3, "PLUS", BSP_LCD_COLOR_GREEN, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_3));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_4, "MINUS", BSP_LCD_COLOR_MAGENTA, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_4));
    }
    else
    {
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_1, "TIME", BSP_LCD_COLOR_BLUE, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_1));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_2,
                                  (g_clock.alarm_enabled != 0U) ? "OFF" : "ON",
                                  BSP_LCD_COLOR_ORANGE,
                                  (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_2));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_3, "STATE", BSP_LCD_COLOR_GREEN, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_3));
        App_Clock_DrawTouchButton(APP_TOUCH_BUTTON_4, "ALARM", BSP_LCD_COLOR_MAGENTA, (uint8_t)(g_touch_ui.active_button == APP_TOUCH_BUTTON_4));
    }

    g_ui_dirty = 0U;
}

static void App_Clock_HandleTouchReleaseAction(AppTouchButtonId_t button_id)
{
    if (g_clock.alarm_ringing != 0U)
    {
        App_Clock_StopAlarm("alarm-stop-touch");
        return;
    }

    if (App_Clock_IsEditView(g_clock.view) != 0U)
    {
        if (button_id == APP_TOUCH_BUTTON_1)
        {
            App_Clock_AdvanceEditView();
        }
        else if (button_id == APP_TOUCH_BUTTON_2)
        {
            App_Clock_CancelEdit("touch-cancel");
        }
        return;
    }

    switch (button_id)
    {
        case APP_TOUCH_BUTTON_1:
            App_Clock_EnterTimeEdit();
            break;

        case APP_TOUCH_BUTTON_2:
            g_clock.alarm_enabled = (uint8_t)!g_clock.alarm_enabled;
            App_Clock_InvalidateUi();
            App_Clock_PrintState("touch-alarm-toggle");
            break;

        case APP_TOUCH_BUTTON_3:
            App_Clock_PrintState("touch-state");
            break;

        case APP_TOUCH_BUTTON_4:
            App_Clock_EnterAlarmEdit();
            break;

        default:
            break;
    }
}

static void App_Clock_HandleTouchHoldAction(AppTouchButtonId_t button_id)
{
    if (g_clock.alarm_ringing != 0U)
    {
        App_Clock_StopAlarm("alarm-stop-touch");
        return;
    }

    if (App_Clock_IsEditView(g_clock.view) == 0U)
    {
        return;
    }

    if (button_id == APP_TOUCH_BUTTON_3)
    {
        App_Clock_AdjustEditValue(1);
    }
    else if (button_id == APP_TOUCH_BUTTON_4)
    {
        App_Clock_AdjustEditValue(-1);
    }
}

static void App_Clock_HandleTouch(void)
{
    BspTouchPoint_t point;
    uint8_t is_pressed;
    uint32_t now_tick;
    AppTouchButtonId_t hit_button;

    now_tick = Drv_Systick_Millis();
    is_pressed = BSP_Touch_GetPoint(&point, BSP_LCD_GetScanMode(), BSP_LCD_GetWidth(), BSP_LCD_GetHeight());
    hit_button = (is_pressed != 0U) ? App_Clock_GetTouchButtonId(&point) : APP_TOUCH_BUTTON_NONE;

    if ((is_pressed != 0U) && (hit_button != APP_TOUCH_BUTTON_NONE))
    {
        if (g_touch_ui.pressed == 0U)
        {
            g_touch_ui.pressed = 1U;
            g_touch_ui.active_button = hit_button;
            g_touch_ui.press_action_sent = 0U;
            g_touch_ui.press_start_tick = now_tick;
            g_touch_ui.last_repeat_tick = now_tick;
            App_Clock_InvalidateUi();

            if ((App_Clock_IsEditView(g_clock.view) != 0U) &&
                ((hit_button == APP_TOUCH_BUTTON_3) || (hit_button == APP_TOUCH_BUTTON_4)))
            {
                App_Clock_HandleTouchHoldAction(hit_button);
                g_touch_ui.press_action_sent = 1U;
            }
        }
        else if ((g_touch_ui.active_button == hit_button) &&
                 (App_Clock_IsEditView(g_clock.view) != 0U) &&
                 ((hit_button == APP_TOUCH_BUTTON_3) || (hit_button == APP_TOUCH_BUTTON_4)))
        {
            if (((now_tick - g_touch_ui.press_start_tick) >= APP_TOUCH_REPEAT_START_MS) &&
                ((now_tick - g_touch_ui.last_repeat_tick) >= APP_TOUCH_REPEAT_PERIOD_MS))
            {
                g_touch_ui.last_repeat_tick = now_tick;
                App_Clock_HandleTouchHoldAction(hit_button);
                g_touch_ui.press_action_sent = 1U;
            }
        }
        return;
    }

    if (g_touch_ui.pressed != 0U)
    {
        AppTouchButtonId_t released_button;
        uint8_t send_release_action;

        released_button = g_touch_ui.active_button;
        send_release_action = (uint8_t)(g_touch_ui.press_action_sent == 0U);

        g_touch_ui.pressed = 0U;
        g_touch_ui.press_action_sent = 0U;
        g_touch_ui.active_button = APP_TOUCH_BUTTON_NONE;
        App_Clock_InvalidateUi();

        if ((released_button != APP_TOUCH_BUTTON_NONE) && (send_release_action != 0U))
        {
            App_Clock_HandleTouchReleaseAction(released_button);
        }
    }
}

void App_Clock_Init(void)
{
    DrvRtcTime_t default_time;

    BSP_LED_Init();
    Drv_Key_Init();
    Drv_DebugUart_Init();
    Drv_Systick_Init();
    BSP_LCD_Init();
    BSP_Touch_Init();

    default_time.hour = 12U;
    default_time.minute = 0U;
    default_time.second = 0U;

    g_clock.rtc_source = Drv_Rtc_Init(&default_time);

    Drv_DebugUart_SendString("Clock project refactor ready.\n");
    printf("RTC source: %s\n", App_Clock_RtcSourceName(g_clock.rtc_source));
    printf("LCD id: 0x%04X, scan=%u, size=%ux%u\n",
           BSP_LCD_GetId(),
           BSP_LCD_GetScanMode(),
           BSP_LCD_GetWidth(),
           BSP_LCD_GetHeight());
    Drv_DebugUart_SendString("Touch buttons ready: TIME / ON-OFF / STATE / ALARM\n");
    Drv_DebugUart_SendString("Edit buttons ready : NEXT / BACK / PLUS / MINUS\n");
    App_Clock_InvalidateUi();
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
    App_Clock_HandleTouch();
    App_Clock_CheckEditTimeout();

    Drv_Rtc_GetTime(&now);
    now_seconds = App_Clock_TimeToSeconds(&now);

    if (now_seconds != last_printed_second)
    {
        last_printed_second = now_seconds;
        App_Clock_InvalidateUi();

        if (g_clock.view == APP_VIEW_RUN)
        {
            App_Clock_PrintState("tick");
        }
    }

    App_Clock_CheckAlarm(&now);
    App_Clock_ApplyLedState();

    if (g_ui_dirty != 0U)
    {
        App_Clock_Render(&now);
    }
}

#include "app_clock_ui.h"

#include <stdio.h>

#include "app_clock_core.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "drv_systick.h"

#define APP_TOUCH_REPEAT_START_MS    400UL
#define APP_TOUCH_REPEAT_PERIOD_MS   150UL

static void AppClockUi_GetButtonRect(AppTouchButtonId_t button_id,
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

static AppTouchButtonId_t AppClockUi_GetTouchButtonId(const BspTouchPoint_t *point)
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

        AppClockUi_GetButtonRect(button_id, &x, &y, &width, &height);
        if ((point->x >= x) && (point->x < (uint16_t)(x + width)) &&
            (point->y >= y) && (point->y < (uint16_t)(y + height)))
        {
            return button_id;
        }
    }

    return APP_TOUCH_BUTTON_NONE;
}

static void AppClockUi_DrawTouchButton(AppTouchButtonId_t button_id,
                                       const char *label,
                                       uint16_t base_color,
                                       uint8_t pressed)
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t fill_color;

    AppClockUi_GetButtonRect(button_id, &x, &y, &width, &height);
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

void AppClockUi_Init(TouchUiState_t *touch_ui)
{
    touch_ui->pressed = 0U;
    touch_ui->press_action_sent = 0U;
    touch_ui->active_button = APP_TOUCH_BUTTON_NONE;
    touch_ui->press_start_tick = 0U;
    touch_ui->last_repeat_tick = 0U;
}

void AppClockUi_HandleTouch(ClockContext_t *clock, TouchUiState_t *touch_ui, uint8_t *ui_dirty)
{
    BspTouchPoint_t point;
    uint8_t is_pressed;
    uint32_t now_tick;
    AppTouchButtonId_t hit_button;

    now_tick = Drv_Systick_Millis();
    is_pressed = BSP_Touch_GetPoint(&point, BSP_LCD_GetScanMode(), BSP_LCD_GetWidth(), BSP_LCD_GetHeight());
    hit_button = (is_pressed != 0U) ? AppClockUi_GetTouchButtonId(&point) : APP_TOUCH_BUTTON_NONE;

    if ((is_pressed != 0U) && (hit_button != APP_TOUCH_BUTTON_NONE))
    {
        if (touch_ui->pressed == 0U)
        {
            touch_ui->pressed = 1U;
            touch_ui->active_button = hit_button;
            touch_ui->press_action_sent = 0U;
            touch_ui->press_start_tick = now_tick;
            touch_ui->last_repeat_tick = now_tick;
            *ui_dirty = 1U;

            if ((AppClockCore_IsEditView(clock->view) != 0U) &&
                ((hit_button == APP_TOUCH_BUTTON_3) || (hit_button == APP_TOUCH_BUTTON_4)))
            {
                AppClockCore_HandleTouchCommand(clock, hit_button, 1U, ui_dirty);
                touch_ui->press_action_sent = 1U;
            }
        }
        else if ((touch_ui->active_button == hit_button) &&
                 (AppClockCore_IsEditView(clock->view) != 0U) &&
                 ((hit_button == APP_TOUCH_BUTTON_3) || (hit_button == APP_TOUCH_BUTTON_4)))
        {
            if (((now_tick - touch_ui->press_start_tick) >= APP_TOUCH_REPEAT_START_MS) &&
                ((now_tick - touch_ui->last_repeat_tick) >= APP_TOUCH_REPEAT_PERIOD_MS))
            {
                touch_ui->last_repeat_tick = now_tick;
                AppClockCore_HandleTouchCommand(clock, hit_button, 1U, ui_dirty);
                touch_ui->press_action_sent = 1U;
            }
        }
        return;
    }

    if (touch_ui->pressed != 0U)
    {
        AppTouchButtonId_t released_button;
        uint8_t send_release_action;

        released_button = touch_ui->active_button;
        send_release_action = (uint8_t)(touch_ui->press_action_sent == 0U);

        touch_ui->pressed = 0U;
        touch_ui->press_action_sent = 0U;
        touch_ui->active_button = APP_TOUCH_BUTTON_NONE;
        *ui_dirty = 1U;

        if ((released_button != APP_TOUCH_BUTTON_NONE) && (send_release_action != 0U))
        {
            AppClockCore_HandleTouchCommand(clock, released_button, 0U, ui_dirty);
        }
    }
}

void AppClockUi_Render(const ClockContext_t *clock,
                       const TouchUiState_t *touch_ui,
                       uint8_t *ui_dirty,
                       const DrvRtcTime_t *now)
{
    char time_string[16];
    char line_buffer[32];
    const DrvRtcTime_t *display_time;
    uint16_t screen_width;
    uint16_t time_bg_color;

    screen_width = BSP_LCD_GetWidth();
    display_time = (AppClockCore_IsEditView(clock->view) != 0U) ? &clock->edit_time : now;

    sprintf(time_string, "%02u:%02u:%02u", display_time->hour, display_time->minute, display_time->second);

    BSP_LCD_FillScreen(BSP_LCD_COLOR_BLACK);

    BSP_LCD_FillRect(0U, 0U, screen_width, 28U, BSP_LCD_COLOR_NAVY);
    BSP_LCD_DrawStringCentered(0U, 7U, screen_width, "CLOCK", BSP_LCD_COLOR_WHITE, BSP_LCD_COLOR_NAVY, 2U);

    time_bg_color = (clock->alarm_ringing != 0U) ? BSP_LCD_COLOR_RED : BSP_LCD_COLOR_DARK_GRAY;
    BSP_LCD_FillRect(12U, 40U, (uint16_t)(screen_width - 24U), 56U, time_bg_color);
    BSP_LCD_DrawStringCentered(12U, 50U, (uint16_t)(screen_width - 24U), time_string, BSP_LCD_COLOR_WHITE, time_bg_color, 6U);

    sprintf(line_buffer, "MODE %s", AppClockCore_ModeLabel(clock));
    BSP_LCD_DrawString(16U, 112U, line_buffer, BSP_LCD_COLOR_CYAN, BSP_LCD_COLOR_BLACK, 2U);

    sprintf(line_buffer,
            "ALARM %s %02u:%02u",
            (clock->alarm_enabled != 0U) ? "ON" : "OFF",
            clock->alarm_time.hour,
            clock->alarm_time.minute);
    BSP_LCD_DrawString(16U, 136U, line_buffer, BSP_LCD_COLOR_YELLOW, BSP_LCD_COLOR_BLACK, 2U);

    sprintf(line_buffer, "RTC %s", AppClockCore_RtcSourceLabel(clock->rtc_source));
    BSP_LCD_DrawString(16U, 160U, line_buffer, BSP_LCD_COLOR_GREEN, BSP_LCD_COLOR_BLACK, 2U);

    if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        sprintf(line_buffer, "FIELD %s", AppClockCore_EditFieldLabel(clock));
        BSP_LCD_DrawString(16U, 184U, line_buffer, BSP_LCD_COLOR_MAGENTA, BSP_LCD_COLOR_BLACK, 2U);
    }
    else if (clock->alarm_ringing != 0U)
    {
        BSP_LCD_DrawString(16U, 184U, "TOUCH STOP", BSP_LCD_COLOR_RED, BSP_LCD_COLOR_BLACK, 2U);
    }
    else
    {
        BSP_LCD_DrawString(16U, 184U, "PHYS KEY OK", BSP_LCD_COLOR_GRAY, BSP_LCD_COLOR_BLACK, 2U);
    }

    if (clock->alarm_ringing != 0U)
    {
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_1, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_1));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_2, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_2));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_3, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_3));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_4, "STOP", BSP_LCD_COLOR_RED, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_4));
    }
    else if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_1, "NEXT", BSP_LCD_COLOR_BLUE, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_1));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_2, "BACK", BSP_LCD_COLOR_ORANGE, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_2));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_3, "PLUS", BSP_LCD_COLOR_GREEN, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_3));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_4, "MINUS", BSP_LCD_COLOR_MAGENTA, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_4));
    }
    else
    {
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_1, "TIME", BSP_LCD_COLOR_BLUE, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_1));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_2,
                                   (clock->alarm_enabled != 0U) ? "OFF" : "ON",
                                   BSP_LCD_COLOR_ORANGE,
                                   (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_2));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_3, "STATE", BSP_LCD_COLOR_GREEN, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_3));
        AppClockUi_DrawTouchButton(APP_TOUCH_BUTTON_4, "ALARM", BSP_LCD_COLOR_MAGENTA, (uint8_t)(touch_ui->active_button == APP_TOUCH_BUTTON_4));
    }

    *ui_dirty = 0U;
}

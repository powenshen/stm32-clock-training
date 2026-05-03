/*
 * LCD rendering and touch interaction for the clock application.
 * This module is intentionally UI-focused and delegates all business rules to app_clock_core.c.
 */
#include "app_clock_ui.h"

#include <stdio.h>

#include "app_clock_core.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "drv_systick.h"

#define APP_TOUCH_REPEAT_START_MS    400UL
#define APP_TOUCH_REPEAT_PERIOD_MS   150UL

/**
 * @brief  计算逻辑触摸按钮的屏幕矩形区域
 * @param  button_id: 逻辑按钮编号
 * @param  x: 按钮左上角 X 坐标输出指针
 * @param  y: 按钮左上角 Y 坐标输出指针
 * @param  width: 按钮宽度输出指针
 * @param  height: 按钮高度输出指针
 * @retval 无
 */
static void AppClockUi_GetButtonRect(AppTouchButtonId_t button_id,
                                     uint16_t *x,
                                     uint16_t *y,
                                     uint16_t *width,
                                     uint16_t *height)
{
    uint16_t screen_width;   /* LCD 当前横向像素宽度 */
    uint16_t screen_height;  /* LCD 当前纵向像素高度 */
    uint16_t margin;         /* 按钮区域外边距 */
    uint16_t gap;            /* 按钮之间的水平间距 */
    uint16_t button_width;   /* 单个按钮宽度 */
    uint16_t button_height;  /* 单个按钮高度 */
    uint16_t base_x;         /* 当前按钮左上角 X 基准坐标 */

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

/**
 * @brief  根据触摸点坐标判断命中的逻辑按钮编号
 * @param  point: 触摸点坐标指针
 * @retval 命中的逻辑按钮编号
 */
static AppTouchButtonId_t AppClockUi_GetTouchButtonId(const BspTouchPoint_t *point)
{
    AppTouchButtonId_t button_id;  /* 当前遍历的逻辑按钮编号 */

    if (point == 0)
    {
        return APP_TOUCH_BUTTON_NONE;
    }

    for (button_id = APP_TOUCH_BUTTON_1; button_id <= APP_TOUCH_BUTTON_4; button_id++)
    {
        uint16_t x;       /* 按钮左上角 X 坐标 */
        uint16_t y;       /* 按钮左上角 Y 坐标 */
        uint16_t width;   /* 按钮宽度 */
        uint16_t height;  /* 按钮高度 */

        AppClockUi_GetButtonRect(button_id, &x, &y, &width, &height);
        if ((point->x >= x) && (point->x < (uint16_t)(x + width)) &&
            (point->y >= y) && (point->y < (uint16_t)(y + height)))
        {
            return button_id;
        }
    }

    return APP_TOUCH_BUTTON_NONE;
}

/**
 * @brief  绘制一个逻辑触摸按钮
 * @param  button_id: 逻辑按钮编号
 * @param  label: 按钮显示文本
 * @param  base_color: 按钮基础颜色
 * @param  pressed: 按钮是否处于按下态
 * @retval 无
 */
static void AppClockUi_DrawTouchButton(AppTouchButtonId_t button_id,
                                       const char *label,
                                       uint16_t base_color,
                                       uint8_t pressed)
{
    uint16_t x;           /* 按钮左上角 X 坐标 */
    uint16_t y;           /* 按钮左上角 Y 坐标 */
    uint16_t width;       /* 按钮宽度 */
    uint16_t height;      /* 按钮高度 */
    uint16_t fill_color;  /* 当前按钮填充颜色 */

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

/**
 * @brief  初始化触摸交互运行时状态
 * @param  touch_ui: 触摸交互状态对象指针
 * @retval 无
 */
void AppClockUi_Init(TouchUiState_t *touch_ui)
{
    touch_ui->pressed = 0U;
    touch_ui->press_action_sent = 0U;
    touch_ui->active_button = APP_TOUCH_BUTTON_NONE;
    touch_ui->press_start_tick = 0U;
    touch_ui->last_repeat_tick = 0U;
}

/**
 * @brief  处理触摸按下与释放，并转换为电子钟逻辑动作
 * @param  clock: 电子钟状态对象指针
 * @param  touch_ui: 触摸交互状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
void AppClockUi_HandleTouch(ClockContext_t *clock,
                            TouchUiState_t *touch_ui,
                            uint8_t *ui_dirty,
                            uint8_t *settings_dirty)
{
    BspTouchPoint_t point;        /* 当前采样到的触摸点坐标 */
    uint8_t is_pressed;           /* 当前是否检测到有效按压 */
    uint32_t now_tick;            /* 当前系统毫秒计数 */
    AppTouchButtonId_t hit_button;/* 当前命中的逻辑按钮编号 */

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
                AppClockCore_HandleTouchCommand(clock, hit_button, 1U, ui_dirty, settings_dirty);
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
                AppClockCore_HandleTouchCommand(clock, hit_button, 1U, ui_dirty, settings_dirty);
                touch_ui->press_action_sent = 1U;
            }
        }
        return;
    }

    if (touch_ui->pressed != 0U)
    {
        AppTouchButtonId_t released_button;  /* 刚刚释放的逻辑按钮编号 */
        uint8_t send_release_action;         /* 是否需要在释放时补发一次逻辑动作 */

        released_button = touch_ui->active_button;
        send_release_action = (uint8_t)(touch_ui->press_action_sent == 0U);

        touch_ui->pressed = 0U;
        touch_ui->press_action_sent = 0U;
        touch_ui->active_button = APP_TOUCH_BUTTON_NONE;
        *ui_dirty = 1U;

        if ((released_button != APP_TOUCH_BUTTON_NONE) && (send_release_action != 0U))
        {
            AppClockCore_HandleTouchCommand(clock, released_button, 0U, ui_dirty, settings_dirty);
        }
    }
}

/**
 * @brief  将当前电子钟状态绘制到 LCD
 * @param  clock: 电子钟状态对象指针
 * @param  touch_ui: 触摸交互状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  now: 当前 RTC 时间指针
 * @retval 无
 */
void AppClockUi_Render(const ClockContext_t *clock,
                       const TouchUiState_t *touch_ui,
                       uint8_t *ui_dirty,
                       const DrvRtcTime_t *now)
{
    char time_string[16];               /* 主时间显示字符串缓存 */
    char line_buffer[32];               /* 普通文本行缓存 */
    const DrvRtcTime_t *display_time;   /* 当前需要显示的时间对象 */
    uint16_t screen_width;              /* LCD 当前横向像素宽度 */
    uint16_t time_bg_color;             /* 主时间区域背景色 */

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
        sprintf(line_buffer, "STOP ALARM  MUTE %s", (clock->mute_enabled != 0U) ? "ON" : "OFF");
        BSP_LCD_DrawString(16U, 184U, line_buffer, BSP_LCD_COLOR_RED, BSP_LCD_COLOR_BLACK, 2U);
    }
    else
    {
        sprintf(line_buffer, "MUTE %s  PHYS+TOUCH+IR", (clock->mute_enabled != 0U) ? "ON" : "OFF");
        BSP_LCD_DrawString(16U, 184U, line_buffer, BSP_LCD_COLOR_GRAY, BSP_LCD_COLOR_BLACK, 2U);
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

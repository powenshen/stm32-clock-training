#include "app_clock.h"

#include <stdio.h>

#include "app_clock_core.h"
#include "app_clock_internal.h"
#include "app_clock_remote.h"
#include "app_clock_storage.h"
#include "app_clock_ui.h"
#include "bsp_buzzer.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "bsp_touch.h"
#include "drv_debug_uart.h"
#include "drv_ir_remote.h"
#include "drv_key.h"
#include "drv_rtc.h"
#include "drv_systick.h"

static ClockContext_t g_clock;          /* 电子钟应用全局状态 */
static TouchUiState_t g_touch_ui;       /* 触摸交互运行时状态 */
static uint8_t g_ui_dirty = 1U;         /* LCD 界面待刷新标志 */
static uint8_t g_settings_dirty = 0U;   /* 参数待保存标志 */

/**
 * @brief  电子钟应用初始化总入口
 * @param  无
 * @retval 无
 */
void App_Clock_Init(void)
{
    DrvRtcTime_t default_time;  /* RTC 初始化默认时间 */

    AppClockCore_InitContext(&g_clock);
    AppClockUi_Init(&g_touch_ui);
    g_ui_dirty = 1U;

    BSP_LED_Init();
    BSP_Buzzer_Init();
    Drv_Key_Init();
    Drv_IrRemote_Init();
    Drv_DebugUart_Init();
    Drv_Systick_Init();
    BSP_LCD_Init();
    BSP_Touch_Init();

    default_time.hour = 12U;
    default_time.minute = 0U;
    default_time.second = 0U;

    g_clock.rtc_source = Drv_Rtc_Init(&default_time);
    AppClockStorage_Init();
    if (AppClockStorage_Load(&g_clock) != 0U)
    {
        Drv_DebugUart_SendString("Clock settings restored from backup.\n");
    }
    else
    {
        Drv_DebugUart_SendString("Clock settings use defaults.\n");
    }

    Drv_DebugUart_SendString("Clock project refactor ready.\n");
    printf("RTC source: %s\n", AppClockCore_RtcSourceName(g_clock.rtc_source));
    printf("LCD id: 0x%04X, scan=%u, size=%ux%u\n",
           BSP_LCD_GetId(),
           BSP_LCD_GetScanMode(),
           BSP_LCD_GetWidth(),
           BSP_LCD_GetHeight());
    Drv_DebugUart_SendString("Buzzer ready: PA8 gate control enabled.\n");
    Drv_DebugUart_SendString("Touch buttons ready: TIME / ON-OFF / STATE / ALARM\n");
    Drv_DebugUart_SendString("Edit buttons ready : NEXT / BACK / PLUS / MINUS\n");
    AppClockCore_PrintState(&g_clock, "boot");
}

/**
 * @brief  电子钟 1ms 周期任务入口
 * @param  无
 * @retval 无
 */
void App_Clock_Task1ms(void)
{
    Drv_Key_Task1ms();
    Drv_Rtc_Task1ms();
}

/**
 * @brief  电子钟主循环任务入口
 * @param  无
 * @retval 无
 */
void App_Clock_Task(void)
{
    static uint32_t last_printed_second = 0xFFFFFFFFUL;  /* 上次输出到串口的秒计数 */
    DrvRtcTime_t now;                                    /* 当前 RTC 时间快照 */
    uint32_t now_seconds;                                /* 当前时间对应的当天秒数 */

    AppClockCore_HandleKeyEvent(&g_clock, Drv_Key_GetEvent(), &g_ui_dirty, &g_settings_dirty);
    AppClockUi_HandleTouch(&g_clock, &g_touch_ui, &g_ui_dirty, &g_settings_dirty);
    AppClockRemote_HandleEvent(&g_clock, Drv_IrRemote_GetEvent(), &g_ui_dirty, &g_settings_dirty);
    AppClockCore_CheckEditTimeout(&g_clock, &g_ui_dirty);

    Drv_Rtc_GetTime(&now);
    now_seconds = AppClockCore_TimeToSeconds(&now);

    if (now_seconds != last_printed_second)
    {
        last_printed_second = now_seconds;
        g_ui_dirty = 1U;

        if (g_clock.view == APP_VIEW_RUN)
        {
            AppClockCore_PrintState(&g_clock, "tick");
        }
    }

    AppClockCore_CheckAlarm(&g_clock, &now, &g_ui_dirty);
    AppClockCore_UpdateIndicators(&g_clock, &g_ui_dirty);

    if (g_settings_dirty != 0U)
    {
        AppClockStorage_Save(&g_clock);
        g_settings_dirty = 0U;
    }

    if (g_ui_dirty != 0U)
    {
        AppClockUi_Render(&g_clock, &g_touch_ui, &g_ui_dirty, &now);
    }
}

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
#include "sim_debug_config.h"

static ClockContext_t g_clock;
static TouchUiState_t g_touch_ui;
static uint8_t g_ui_dirty = 1U;
static uint8_t g_settings_dirty = 0U;

void App_Clock_Init(void)
{
    DrvRtcTime_t default_time;

    AppClockCore_InitContext(&g_clock);
    AppClockUi_Init(&g_touch_ui);
    g_ui_dirty = 1U;
    g_settings_dirty = 0U;

    BSP_LED_Init();
    BSP_Buzzer_Init();
    Drv_Key_Init();
    Drv_Systick_Init();

#if !APP_CLOCK_SIM_ENABLED
    Drv_IrRemote_Init();
    Drv_DebugUart_Init();
    BSP_LCD_Init();
    BSP_Touch_Init();
#endif

    default_time.hour = 12U;
    default_time.minute = 0U;
    default_time.second = 0U;

    g_clock.rtc_source = Drv_Rtc_Init(&default_time);

#if !APP_CLOCK_SIM_ENABLED
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
#endif

    AppClockCore_PrintState(&g_clock, APP_CLOCK_SIM_ENABLED ? "sim-boot" : "boot");
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

    AppClockCore_HandleKeyEvent(&g_clock, Drv_Key_GetEvent(), &g_ui_dirty, &g_settings_dirty);

#if !APP_CLOCK_SIM_ENABLED
    AppClockUi_HandleTouch(&g_clock, &g_touch_ui, &g_ui_dirty, &g_settings_dirty);
    AppClockRemote_HandleEvent(&g_clock, Drv_IrRemote_GetEvent(), &g_ui_dirty, &g_settings_dirty);
#endif

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
#if !APP_CLOCK_SIM_ENABLED
        AppClockStorage_Save(&g_clock);
#endif
        g_settings_dirty = 0U;
    }

#if !APP_CLOCK_SIM_ENABLED
    if (g_ui_dirty != 0U)
    {
        AppClockUi_Render(&g_clock, &g_touch_ui, &g_ui_dirty, &now);
    }
#endif
}

void App_Clock_GetDebugSnapshot(AppClockDebugSnapshot_t *snapshot)
{
    if (snapshot == 0)
    {
        return;
    }

    Drv_Rtc_GetTime(&snapshot->now);
    snapshot->alarm_time = g_clock.alarm_time;
    snapshot->edit_time = g_clock.edit_time;
    snapshot->systick_ms = Drv_Systick_Millis();
    snapshot->last_activity_tick = g_clock.last_activity_tick;
    snapshot->alarm_start_tick = g_clock.alarm_start_tick;
    snapshot->view = (uint8_t)g_clock.view;
    snapshot->rtc_source = (uint8_t)g_clock.rtc_source;
    snapshot->alarm_enabled = g_clock.alarm_enabled;
    snapshot->alarm_ringing = g_clock.alarm_ringing;
    snapshot->mute_enabled = g_clock.mute_enabled;
    snapshot->blink_state = g_clock.blink_state;
    snapshot->led_mask = BSP_LED_GetMask();
    snapshot->buzzer_on = BSP_Buzzer_GetState();
}

#include "app_clock.h"

#include "app_clock_core.h"
#include "app_clock_internal.h"
#include "app_clock_remote.h"
#include "app_clock_storage.h"
#include "app_clock_ui.h"
#include "bsp_buzzer.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "bsp_touch.h"
#include "drv_ir_remote.h"
#include "drv_rtc.h"
#include "drv_systick.h"

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
    Drv_Systick_Init();

    Drv_IrRemote_Init();
    BSP_LCD_Init();
    BSP_Touch_Init();

    default_time.hour = 12U;
    default_time.minute = 0U;
    default_time.second = 0U;

    g_clock.rtc_source = Drv_Rtc_Init(&default_time);

    AppClockStorage_Init();
    AppClockStorage_Load(&g_clock);
}

void App_Clock_Task1ms(void)
{
    Drv_Rtc_Task1ms();
}

void App_Clock_Task(void)
{
    DrvRtcTime_t now;

    AppClockUi_HandleTouch(&g_clock, &g_touch_ui, &g_ui_dirty, &g_settings_dirty);
    AppClockRemote_HandleEvent(&g_clock, Drv_IrRemote_GetEvent(), &g_ui_dirty, &g_settings_dirty);

    AppClockCore_CheckEditTimeout(&g_clock, &g_ui_dirty);

    Drv_Rtc_GetTime(&now);

    AppClockCore_CheckAlarm(&g_clock, &now, &g_ui_dirty);
    AppClockCore_CheckHourlyChime(&g_clock, &now);
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

#include "app_clock_sim.h"

#include <string.h>

#include "app_clock_internal.h"
#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "drv_systick.h"
#include "sim_debug_config.h"

#define APP_SIM_KEY_SETTLE_MS       25U
#define APP_SIM_KEY_LONG_HOLD_MS    820U

volatile AppClockSimReport_t g_app_clock_sim_report;

#define APP_CLOCK_SIM_CHECK(field, condition)                     \
    do                                                           \
    {                                                            \
        g_app_clock_sim_report.total_checks++;                   \
        g_app_clock_sim_report.field = ((condition) != 0U);      \
        if (g_app_clock_sim_report.field == 0U)                  \
        {                                                        \
            g_app_clock_sim_report.failed_checks++;              \
            g_app_clock_sim_report.all_passed = 0U;              \
        }                                                        \
    } while (0)

static uint8_t AppClockSim_TimeEquals(const DrvRtcTime_t *time,
                                      uint8_t hour,
                                      uint8_t minute,
                                      uint8_t second)
{
    return (uint8_t)((time->hour == hour) &&
                     (time->minute == minute) &&
                     (time->second == second));
}

static void AppClockSim_SetRtcTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    DrvRtcTime_t time;

    time.hour = hour;
    time.minute = minute;
    time.second = second;
    Drv_Rtc_SetTime(&time);
}

static void AppClockSim_ResetReport(void)
{
    memset((void *)&g_app_clock_sim_report, 0, sizeof(g_app_clock_sim_report));
    g_app_clock_sim_report.all_passed = 1U;
}

static void AppClockSim_KeyDriverTaskMs(uint32_t duration_ms)
{
    uint32_t elapsed_ms;

    for (elapsed_ms = 0U; elapsed_ms < duration_ms; elapsed_ms++)
    {
        Drv_Key_Task1ms();
    }
}

static void AppClockSim_RunKeyDriverChecks(void)
{
    KeyEvent_t event;

    Drv_Key_Init();
    Drv_Key_SimSetLevel(KEY_ID_KEY1, 1U);
    AppClockSim_KeyDriverTaskMs(APP_SIM_KEY_SETTLE_MS);
    Drv_Key_SimSetLevel(KEY_ID_KEY1, 0U);
    AppClockSim_KeyDriverTaskMs(APP_SIM_KEY_SETTLE_MS);
    event = Drv_Key_GetEvent();
    g_app_clock_sim_report.last_key_event = event;
    APP_CLOCK_SIM_CHECK(key_click_ok,
                        (event.key_id == KEY_ID_KEY1) && (event.type == KEY_EVENT_TYPE_CLICK));

    Drv_Key_Init();
    Drv_Key_SimSetLevel(KEY_ID_KEY2, 1U);
    AppClockSim_KeyDriverTaskMs(APP_SIM_KEY_SETTLE_MS + APP_SIM_KEY_LONG_HOLD_MS);
    event = Drv_Key_GetEvent();
    g_app_clock_sim_report.last_key_event = event;
    APP_CLOCK_SIM_CHECK(key_long_press_ok,
                        (event.key_id == KEY_ID_KEY2) && (event.type == KEY_EVENT_TYPE_LONG_PRESS));

    AppClockSim_KeyDriverTaskMs(200U);
    event = Drv_Key_GetEvent();
    g_app_clock_sim_report.last_key_event = event;
    APP_CLOCK_SIM_CHECK(key_repeat_ok,
                        (event.key_id == KEY_ID_KEY2) && (event.type == KEY_EVENT_TYPE_REPEAT));

    Drv_Key_SimSetLevel(KEY_ID_KEY2, 0U);
    AppClockSim_KeyDriverTaskMs(APP_SIM_KEY_SETTLE_MS);
    (void)Drv_Key_GetEvent();
}

static void AppClockSim_RunTimeEditChecks(void)
{
    uint8_t previous_minute;

    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 34U, 56U);
    App_Clock_Task();

    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_enter_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_SET_TIME_HOUR);
    APP_CLOCK_SIM_CHECK(edit_led_ok,
                        g_app_clock_sim_report.last_snapshot.led_mask == BSP_LED_BLUE_MASK);

    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_field_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_SET_TIME_MINUTE);

    previous_minute = g_app_clock_sim_report.last_snapshot.edit_time.minute;
    AppClockSim_KeyClick(KEY_ID_KEY2);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_inc_ok,
                        g_app_clock_sim_report.last_snapshot.edit_time.minute ==
                            (uint8_t)((previous_minute + 1U) % 60U));

    previous_minute = g_app_clock_sim_report.last_snapshot.edit_time.minute;
    AppClockSim_KeyHold(KEY_ID_KEY2, APP_SIM_KEY_LONG_HOLD_MS);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_dec_ok,
                        g_app_clock_sim_report.last_snapshot.edit_time.minute ==
                            (uint8_t)((previous_minute == 0U) ? 59U : (previous_minute - 1U)));

    AppClockSim_KeyHold(KEY_ID_KEY1, APP_SIM_KEY_LONG_HOLD_MS);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_cancel_ok,
                        (g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN) &&
                        AppClockSim_TimeEquals(&g_app_clock_sim_report.last_snapshot.now, 12U, 34U, 56U));
}

static void AppClockSim_RunEditTimeoutCheck(void)
{
    App_Clock_Init();
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_StepMs(10001U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(edit_timeout_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN);
}

static void AppClockSim_EnableDefaultAlarmAt(uint8_t hour, uint8_t minute, uint8_t second)
{
    App_Clock_Init();
    AppClockSim_SetRtcTime(hour, minute, second);
    App_Clock_Task();
    AppClockSim_KeyHold(KEY_ID_KEY1, APP_SIM_KEY_LONG_HOLD_MS);
}

static void AppClockSim_RunAlarmTriggerAndStopChecks(void)
{
    AppClockSim_EnableDefaultAlarmAt(6U, 29U, 59U);

    AppClockSim_StepMs(1000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_trigger_ok,
                        g_app_clock_sim_report.last_snapshot.alarm_ringing != 0U);
    APP_CLOCK_SIM_CHECK(alarm_output_ok,
                        (g_app_clock_sim_report.last_snapshot.led_mask == BSP_LED_RED_MASK) &&
                        (g_app_clock_sim_report.last_snapshot.buzzer_on == 1U) &&
                        (BSP_Buzzer_GetState() == 1U));

    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_stop_ok,
                        (g_app_clock_sim_report.last_snapshot.alarm_ringing == 0U) &&
                        (g_app_clock_sim_report.last_snapshot.buzzer_on == 0U));
}

static void AppClockSim_RunAlarmTimeoutCheck(void)
{
    AppClockSim_EnableDefaultAlarmAt(6U, 29U, 59U);

    AppClockSim_StepMs(1000U);
    AppClockSim_StepMs(30001U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_timeout_ok,
                        (g_app_clock_sim_report.last_snapshot.alarm_ringing == 0U) &&
                        (g_app_clock_sim_report.last_snapshot.buzzer_on == 0U));
}

void AppClockSim_RefreshSnapshot(void)
{
    AppClockDebugSnapshot_t snapshot;

    App_Clock_GetDebugSnapshot(&snapshot);
    g_app_clock_sim_report.last_snapshot = snapshot;
}

void AppClockSim_StepMs(uint32_t duration_ms)
{
    uint32_t elapsed_ms;

    for (elapsed_ms = 0U; elapsed_ms < duration_ms; elapsed_ms++)
    {
        Drv_Systick_IrqHandler();
        App_Clock_Task1ms();
        App_Clock_Task();
    }
}

void AppClockSim_KeyClick(KeyId_t key_id)
{
    Drv_Key_SimSetLevel(key_id, 1U);
    AppClockSim_StepMs(APP_SIM_KEY_SETTLE_MS);
    Drv_Key_SimSetLevel(key_id, 0U);
    AppClockSim_StepMs(APP_SIM_KEY_SETTLE_MS);
}

void AppClockSim_KeyHold(KeyId_t key_id, uint32_t hold_ms)
{
    Drv_Key_SimSetLevel(key_id, 1U);
    AppClockSim_StepMs(APP_SIM_KEY_SETTLE_MS + hold_ms);
    Drv_Key_SimSetLevel(key_id, 0U);
    AppClockSim_StepMs(APP_SIM_KEY_SETTLE_MS);
}

void AppClockSim_RunAll(void)
{
    AppClockSim_ResetReport();
    AppClockSim_RunKeyDriverChecks();
    AppClockSim_RunTimeEditChecks();
    AppClockSim_RunEditTimeoutCheck();
    AppClockSim_RunAlarmTriggerAndStopChecks();
    AppClockSim_RunAlarmTimeoutCheck();
    AppClockSim_RefreshSnapshot();
    g_app_clock_sim_report.completed = 1U;
}

void AppClockSim_Main(void)
{
    App_Clock_Init();
    AppClockSim_RefreshSnapshot();

#if APP_CLOCK_SIM_AUTO_RUN
    AppClockSim_RunAll();
#endif

    while (1)
    {
    }
}

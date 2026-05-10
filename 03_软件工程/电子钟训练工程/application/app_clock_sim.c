#include "app_clock_sim.h"

#include <string.h>

#include "app_clock_core.h"
#include "app_clock_internal.h"
#include "app_clock_storage.h"
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
    DrvRtcTime_t now_time;
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
    now_time = g_app_clock_sim_report.last_snapshot.now;
    APP_CLOCK_SIM_CHECK(time_edit_cancel_ok,
                        (g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN) &&
                        AppClockSim_TimeEquals(&now_time, 12U, 34U, 56U));
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
    AppClockSim_StepMs(60001U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_timeout_ok,
                        (g_app_clock_sim_report.last_snapshot.alarm_ringing == 0U) &&
                        (g_app_clock_sim_report.last_snapshot.buzzer_on == 0U));
}

static void AppClockSim_RunRtcDateChecks(void)
{
    DrvRtcDate_t date_before;
    DrvRtcDate_t test_date;

    /* T1.1: RTC second counting */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    AppClockSim_StepMs(3000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(rtc_tick_ok,
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 12U, 0U, 3U));

    /* T1.2: minute carry */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 59U);
    AppClockSim_StepMs(1000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(rtc_carry_min_ok,
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 12U, 1U, 0U));

    /* T1.3: hour carry */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 59U, 59U);
    AppClockSim_StepMs(1000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(rtc_carry_hour_ok,
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 13U, 0U, 0U));

    /* T1.4 + T1.5: midnight rollover + date advance */
    App_Clock_Init();
    AppClockSim_SetRtcTime(23U, 59U, 59U);
    AppClockSim_RefreshSnapshot();
    date_before = g_app_clock_sim_report.last_snapshot.date;
    AppClockSim_StepMs(1000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(rtc_midnight_ok,
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 0U, 0U, 0U));
    APP_CLOCK_SIM_CHECK(date_advance_ok,
                        (g_app_clock_sim_report.last_snapshot.date.year != date_before.year) ||
                        (g_app_clock_sim_report.last_snapshot.date.month != date_before.month) ||
                        (g_app_clock_sim_report.last_snapshot.date.day != date_before.day));

    /* T2.1: date validity */
    APP_CLOCK_SIM_CHECK(date_valid_ok,
                        (g_app_clock_sim_report.last_snapshot.date.year >= 2026U) &&
                        (g_app_clock_sim_report.last_snapshot.date.month >= 1U) &&
                        (g_app_clock_sim_report.last_snapshot.date.month <= 12U) &&
                        (g_app_clock_sim_report.last_snapshot.date.day >= 1U) &&
                        (g_app_clock_sim_report.last_snapshot.date.day <= 31U));

    /* T1.6: day of week (2026-05-10 = Sunday = 0) */
    test_date.year = 2026U;
    test_date.month = 5U;
    test_date.day = 10U;
    APP_CLOCK_SIM_CHECK(dow_ok, Drv_Rtc_GetDayOfWeek(&test_date) == 0U);

    /* T1.7: battery check in sim mode returns 0 */
    APP_CLOCK_SIM_CHECK(bat_check_ok, Drv_Rtc_WasBackupLost() == 0U);
}

static void AppClockSim_RunTimeSaveCheck(void)
{
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 34U, 56U);
    App_Clock_Task();

    /* Enter TIME SET: HOUR -> MINUTE -> SECOND */
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_KeyClick(KEY_ID_KEY1);
    /* Now in SET_TIME_SECOND, edit_time = 12:34:56 */
    /* KEY1 click in last field saves and returns to RUN */
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(time_edit_save_ok,
                        (g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN) &&
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 12U, 34U, 56U));
}

static void AppClockSim_RunAlarmEditChecks(void)
{
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();

    /* T2.4: enter alarm edit via KEY2 long press */
    AppClockSim_KeyHold(KEY_ID_KEY2, APP_SIM_KEY_LONG_HOLD_MS);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_edit_enter_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_SET_ALARM_HOUR);

    /* T2.5: advance to MINUTE then save */
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_edit_save_ok,
                        (g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN) &&
                        (g_app_clock_sim_report.last_snapshot.alarm_enabled != 0U));

    /* T2.6: alarm disabled LED = BLUE */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_indicator_off_ok,
                        g_app_clock_sim_report.last_snapshot.led_mask == BSP_LED_BLUE_MASK);

    /* T2.7: alarm enabled LED = GREEN */
    AppClockSim_KeyHold(KEY_ID_KEY1, APP_SIM_KEY_LONG_HOLD_MS);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(alarm_indicator_on_ok,
                        g_app_clock_sim_report.last_snapshot.led_mask == BSP_LED_GREEN_MASK);
}

static void AppClockSim_RunTouchChecks(void)
{
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 34U, 56U);
    App_Clock_Task();

    /* Enter TIME SET HOUR */
    AppClockSim_KeyClick(KEY_ID_KEY1);
    /* Initial hour = 12 */

    /* T3.1: touch BTN3 hold = increment */
    App_Clock_SimInjectTouch(APP_TOUCH_BUTTON_3, 1U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(touch_plus_ok,
                        g_app_clock_sim_report.last_snapshot.edit_time.hour == 13U);

    /* T3.2: touch BTN4 hold = decrement */
    App_Clock_SimInjectTouch(APP_TOUCH_BUTTON_4, 1U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(touch_minus_ok,
                        g_app_clock_sim_report.last_snapshot.edit_time.hour == 12U);

    /* T3.3: touch BTN2 (右移) = advance to MINUTE */
    App_Clock_SimInjectTouch(APP_TOUCH_BUTTON_2, 0U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(touch_next_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_SET_TIME_MINUTE);

    /* T3.4: touch BTN1 (模式键) = advance to SECOND (not last field yet) */
    App_Clock_SimInjectTouch(APP_TOUCH_BUTTON_1, 0U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(touch_mode_ok,
                        g_app_clock_sim_report.last_snapshot.view == APP_VIEW_SET_TIME_SECOND);

    /* T3.5: touch BTN1 long = cancel, return to RUN */
    App_Clock_SimInjectTouch(APP_TOUCH_BUTTON_1, 1U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(touch_cancel_ok,
                        (g_app_clock_sim_report.last_snapshot.view == APP_VIEW_RUN) &&
                        AppClockSim_TimeEquals((const DrvRtcTime_t *)&g_app_clock_sim_report.last_snapshot.now, 12U, 34U, 56U));
}

static void AppClockSim_RunChimeAndKeySoundChecks(void)
{
    /* T4.1-T4.3: hourly chime */
    App_Clock_Init();
    AppClockSim_SetRtcTime(11U, 59U, 59U);
    App_Clock_Task();
    AppClockSim_StepMs(1000U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(chime_trigger_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 1U);

    AppClockSim_StepMs(200U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(chime_active_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 1U);

    AppClockSim_StepMs(200U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(chime_end_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 0U);

    /* T4.4-T4.6: key sound + mute */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();

    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(key_sound_trigger_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 1U);

    AppClockSim_StepMs(100U);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(key_sound_end_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 0U);

    /* Enable mute via KEY2 click in RUN mode, then test key sound suppressed */
    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();
    AppClockSim_KeyClick(KEY_ID_KEY2);
    AppClockSim_KeyClick(KEY_ID_KEY1);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(mute_key_sound_ok,
                        g_app_clock_sim_report.last_snapshot.buzzer_on == 0U);
}

static void AppClockSim_RunIrNumberEntryCheck(void)
{
    uint8_t dummy_ui = 0U;
    uint8_t dummy_s = 0U;
    ClockContext_t *ctx;

    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();

    /* Enter TIME SET HOUR */
    AppClockSim_KeyClick(KEY_ID_KEY1);

    ctx = App_Clock_GetSimContext();
    /* Input digits 1 then 3 → hour should be 13 */
    AppClockCore_HandleNumberEntry(ctx, 1U, &dummy_ui, &dummy_s);
    AppClockCore_HandleNumberEntry(ctx, 3U, &dummy_ui, &dummy_s);
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(ir_number_entry_ok,
                        g_app_clock_sim_report.last_snapshot.edit_time.hour == 13U);
}

static void AppClockSim_RunStorageChecks(void)
{
    ClockContext_t *ctx;

    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();

    ctx = App_Clock_GetSimContext();
    ctx->alarm_time.hour = 7U;
    ctx->alarm_time.minute = 15U;
    ctx->alarm_enabled = 1U;
    ctx->mute_enabled = 1U;
    AppClockStorage_Save(ctx);

    /* Corrupt values, then load */
    ctx->alarm_time.hour = 0U;
    ctx->alarm_time.minute = 0U;
    ctx->alarm_enabled = 0U;
    ctx->mute_enabled = 0U;
    AppClockStorage_Load(ctx);

    APP_CLOCK_SIM_CHECK(storage_save_load_ok,
                        (ctx->alarm_time.hour == 7U) &&
                        (ctx->alarm_time.minute == 15U) &&
                        (ctx->alarm_enabled == 1U) &&
                        (ctx->mute_enabled == 1U));

    /* T6.2: sim mode uses SOFTWARE RTC */
    App_Clock_Init();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(storage_sim_bypass_ok,
                        g_app_clock_sim_report.last_snapshot.rtc_source == DRV_RTC_SOURCE_SOFTWARE);
}

static void AppClockSim_RunLcdAndMuteChecks(void)
{
    uint8_t dummy_ui = 0U;
    uint8_t dummy_s = 0U;
    ClockContext_t *ctx;

    App_Clock_Init();
    AppClockSim_SetRtcTime(12U, 0U, 0U);
    App_Clock_Task();

    ctx = App_Clock_GetSimContext();

    /* T7.1: LCD off */
    AppClockCore_ToggleLcdPower(ctx, &dummy_ui);
    App_Clock_Task();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(lcd_power_off_ok,
                        g_app_clock_sim_report.last_snapshot.lcd_on == 0U);

    /* T7.2: LCD on */
    AppClockCore_ToggleLcdPower(ctx, &dummy_ui);
    App_Clock_Task();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(lcd_power_on_ok,
                        g_app_clock_sim_report.last_snapshot.lcd_on == 1U);

    /* T7.3: mute ON */
    AppClockCore_ToggleMute(ctx, &dummy_ui, &dummy_s, "test");
    App_Clock_Task();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(mute_toggle_on_ok,
                        g_app_clock_sim_report.last_snapshot.mute_enabled == 1U);

    /* T7.4: mute OFF */
    AppClockCore_ToggleMute(ctx, &dummy_ui, &dummy_s, "test");
    App_Clock_Task();
    AppClockSim_RefreshSnapshot();
    APP_CLOCK_SIM_CHECK(mute_toggle_off_ok,
                        g_app_clock_sim_report.last_snapshot.mute_enabled == 0U);
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
    AppClockSim_RunRtcDateChecks();
    AppClockSim_RunTimeSaveCheck();
    AppClockSim_RunAlarmEditChecks();
    AppClockSim_RunTouchChecks();
    AppClockSim_RunChimeAndKeySoundChecks();
    AppClockSim_RunIrNumberEntryCheck();
    AppClockSim_RunStorageChecks();
    AppClockSim_RunLcdAndMuteChecks();
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

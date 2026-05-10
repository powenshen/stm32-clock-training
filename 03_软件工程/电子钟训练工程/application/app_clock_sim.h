#ifndef APP_CLOCK_SIM_H
#define APP_CLOCK_SIM_H

#include "app_clock.h"
#include "drv_key.h"
#include "stm32f10x.h"

typedef struct
{
    uint8_t completed;
    uint8_t all_passed;
    uint8_t total_checks;
    uint8_t failed_checks;
    uint8_t key_click_ok;
    uint8_t key_long_press_ok;
    uint8_t key_repeat_ok;
    uint8_t time_edit_enter_ok;
    uint8_t time_edit_field_ok;
    uint8_t time_edit_inc_ok;
    uint8_t time_edit_dec_ok;
    uint8_t time_edit_cancel_ok;
    uint8_t edit_timeout_ok;
    uint8_t edit_led_ok;
    uint8_t alarm_trigger_ok;
    uint8_t alarm_stop_ok;
    uint8_t alarm_timeout_ok;
    uint8_t alarm_output_ok;
    uint8_t rtc_tick_ok;
    uint8_t rtc_carry_min_ok;
    uint8_t rtc_carry_hour_ok;
    uint8_t rtc_midnight_ok;
    uint8_t date_advance_ok;
    uint8_t date_valid_ok;
    uint8_t dow_ok;
    uint8_t bat_check_ok;
    uint8_t time_edit_save_ok;
    uint8_t alarm_edit_enter_ok;
    uint8_t alarm_edit_save_ok;
    uint8_t alarm_indicator_off_ok;
    uint8_t alarm_indicator_on_ok;
    uint8_t touch_plus_ok;
    uint8_t touch_minus_ok;
    uint8_t touch_next_ok;
    uint8_t touch_mode_ok;
    uint8_t touch_cancel_ok;
    uint8_t chime_trigger_ok;
    uint8_t chime_active_ok;
    uint8_t chime_end_ok;
    uint8_t key_sound_trigger_ok;
    uint8_t key_sound_end_ok;
    uint8_t mute_key_sound_ok;
    uint8_t ir_number_entry_ok;
    uint8_t storage_save_load_ok;
    uint8_t storage_sim_bypass_ok;
    uint8_t lcd_power_off_ok;
    uint8_t lcd_power_on_ok;
    uint8_t mute_toggle_on_ok;
    uint8_t mute_toggle_off_ok;
    KeyEvent_t last_key_event;
    AppClockDebugSnapshot_t last_snapshot;
} AppClockSimReport_t;

extern volatile AppClockSimReport_t g_app_clock_sim_report;

void AppClockSim_Main(void);
void AppClockSim_RunAll(void);
void AppClockSim_StepMs(uint32_t duration_ms);
void AppClockSim_KeyClick(KeyId_t key_id);
void AppClockSim_KeyHold(KeyId_t key_id, uint32_t hold_ms);
void AppClockSim_RefreshSnapshot(void);

#endif

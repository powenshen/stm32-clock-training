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

/*
 * Core clock business logic.
 * This module owns mode transitions, alarm rules, indicator state, and source-independent user actions.
 */
#ifndef APP_CLOCK_CORE_H
#define APP_CLOCK_CORE_H

#include "app_clock_internal.h"
#include "drv_key.h"

void AppClockCore_InitContext(ClockContext_t *clock);
uint8_t AppClockCore_IsEditView(AppView_t view);
uint32_t AppClockCore_TimeToSeconds(const DrvRtcTime_t *time);

const char *AppClockCore_ViewName(AppView_t view);
const char *AppClockCore_RtcSourceName(DrvRtcSource_t source);
const char *AppClockCore_RtcSourceLabel(DrvRtcSource_t source);
const char *AppClockCore_ModeLabel(const ClockContext_t *clock);
const char *AppClockCore_EditFieldLabel(const ClockContext_t *clock);

void AppClockCore_PrintState(const ClockContext_t *clock, const char *reason);
void AppClockCore_HandleKeyEvent(ClockContext_t *clock,
                                 KeyEvent_t event,
                                 uint8_t *ui_dirty,
                                 uint8_t *settings_dirty);
void AppClockCore_HandleTouchCommand(ClockContext_t *clock,
                                     AppTouchButtonId_t button_id,
                                     uint8_t is_hold_action,
                                     uint8_t *ui_dirty,
                                     uint8_t *settings_dirty);
void AppClockCore_CheckEditTimeout(ClockContext_t *clock, uint8_t *ui_dirty);
void AppClockCore_CheckAlarm(ClockContext_t *clock, const DrvRtcTime_t *now, uint8_t *ui_dirty);
void AppClockCore_UpdateIndicators(ClockContext_t *clock, uint8_t *ui_dirty);
void AppClockCore_ToggleAlarmEnable(ClockContext_t *clock,
                                    uint8_t *ui_dirty,
                                    uint8_t *settings_dirty,
                                    const char *reason);
void AppClockCore_ToggleMute(ClockContext_t *clock,
                             uint8_t *ui_dirty,
                             uint8_t *settings_dirty,
                             const char *reason);
void AppClockCore_TriggerKeySound(ClockContext_t *clock);
void AppClockCore_CheckHourlyChime(ClockContext_t *clock, const DrvRtcTime_t *now);
void AppClockCore_ToggleLcdPower(ClockContext_t *clock, uint8_t *ui_dirty);
void AppClockCore_HandleNumberEntry(ClockContext_t *clock,
                                    uint8_t digit,
                                    uint8_t *ui_dirty,
                                    uint8_t *settings_dirty);

#endif

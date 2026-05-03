/*
 * Core state machine for the STM32 electronic clock.
 * Input sources such as keys, touch, and infrared should map into this module instead of duplicating rules.
 */
#include "app_clock_core.h"

#include <stdio.h>

#include "bsp_buzzer.h"
#include "bsp_led.h"
#include "drv_rtc.h"
#include "drv_systick.h"

#define APP_EDIT_TIMEOUT_MS          10000UL
#define APP_ALARM_RING_MS            30000UL
#define APP_ALARM_BLINK_PERIOD_MS    200UL

/**
 * @brief  置位界面脏标志，通知主循环刷新 LCD
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_InvalidateUi(uint8_t *ui_dirty)
{
    *ui_dirty = 1U;
}

/**
 * @brief  置位参数脏标志，通知主循环执行一次保存
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
static void AppClockCore_InvalidateSettings(uint8_t *settings_dirty)
{
    *settings_dirty = 1U;
}

/**
 * @brief  记录最近一次编辑活动时间
 * @param  clock: 电子钟状态对象指针
 * @retval 无
 */
static void AppClockCore_RecordActivity(ClockContext_t *clock)
{
    clock->last_activity_tick = Drv_Systick_Millis();
}

/**
 * @brief  进入改时模式并装载当前 RTC 时间
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_EnterTimeEdit(ClockContext_t *clock, uint8_t *ui_dirty)
{
    Drv_Rtc_GetTime(&clock->edit_time);
    clock->view = APP_VIEW_SET_TIME_HOUR;
    AppClockCore_RecordActivity(clock);
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, "enter-time-edit");
}

/**
 * @brief  进入闹钟设置模式并装载当前闹钟时间
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_EnterAlarmEdit(ClockContext_t *clock, uint8_t *ui_dirty)
{
    clock->edit_time = clock->alarm_time;
    clock->view = APP_VIEW_SET_ALARM_HOUR;
    AppClockCore_RecordActivity(clock);
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, "enter-alarm-edit");
}

/**
 * @brief  取消当前编辑并返回运行界面
 * @param  clock: 电子钟状态对象指针
 * @param  reason: 调试输出原因字符串
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_CancelEdit(ClockContext_t *clock, const char *reason, uint8_t *ui_dirty)
{
    clock->view = APP_VIEW_RUN;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, reason);
}

/**
 * @brief  保存编辑后的当前时间到 RTC 驱动
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_SaveTimeEdit(ClockContext_t *clock, uint8_t *ui_dirty)
{
    Drv_Rtc_SetTime(&clock->edit_time);
    clock->view = APP_VIEW_RUN;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, "save-time");
}

/**
 * @brief  保存编辑后的闹钟时间并标记参数待持久化
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
static void AppClockCore_SaveAlarmEdit(ClockContext_t *clock,
                                       uint8_t *ui_dirty,
                                       uint8_t *settings_dirty)
{
    clock->alarm_time = clock->edit_time;
    clock->alarm_time.second = 0U;
    clock->alarm_enabled = 1U;
    clock->view = APP_VIEW_RUN;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_InvalidateSettings(settings_dirty);
    AppClockCore_PrintState(clock, "save-alarm");
}

/**
 * @brief  调整当前编辑字段的值，超出范围时回绕
 * @param  clock: 电子钟状态对象指针
 * @param  delta: 调整方向，正数加一，负数减一
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_AdjustEditValue(ClockContext_t *clock, int8_t delta, uint8_t *ui_dirty)
{
    uint8_t *value;  /* 当前被编辑字段的值指针 */
    uint8_t limit;   /* 当前字段的取值上限 */

    value = 0;
    limit = 0U;

    switch (clock->view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_ALARM_HOUR:
            value = &clock->edit_time.hour;
            limit = 24U;
            break;

        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_ALARM_MINUTE:
            value = &clock->edit_time.minute;
            limit = 60U;
            break;

        case APP_VIEW_SET_TIME_SECOND:
            value = &clock->edit_time.second;
            limit = 60U;
            break;

        default:
            return;
    }

    if (delta > 0)
    {
        *value = (uint8_t)((*value + 1U) % limit);
    }
    else
    {
        *value = (uint8_t)((*value == 0U) ? (limit - 1U) : (*value - 1U));
    }

    /* Alarm editing only keeps hour/minute precision. */
    if ((clock->view == APP_VIEW_SET_ALARM_HOUR) || (clock->view == APP_VIEW_SET_ALARM_MINUTE))
    {
        clock->edit_time.second = 0U;
    }

    AppClockCore_RecordActivity(clock);
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, (delta > 0) ? "adjust++" : "adjust--");
}

/**
 * @brief  推进当前编辑流程，改时依次编辑时分秒，闹钟依次编辑时分
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
static void AppClockCore_AdvanceEditView(ClockContext_t *clock,
                                         uint8_t *ui_dirty,
                                         uint8_t *settings_dirty)
{
    switch (clock->view)
    {
        case APP_VIEW_SET_TIME_HOUR:
            clock->view = APP_VIEW_SET_TIME_MINUTE;
            break;

        case APP_VIEW_SET_TIME_MINUTE:
            clock->view = APP_VIEW_SET_TIME_SECOND;
            break;

        case APP_VIEW_SET_TIME_SECOND:
            AppClockCore_SaveTimeEdit(clock, ui_dirty);
            return;

        case APP_VIEW_SET_ALARM_HOUR:
            clock->view = APP_VIEW_SET_ALARM_MINUTE;
            break;

        case APP_VIEW_SET_ALARM_MINUTE:
            AppClockCore_SaveAlarmEdit(clock, ui_dirty, settings_dirty);
            return;

        default:
            return;
    }

    AppClockCore_RecordActivity(clock);
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, "next-field");
}

/**
 * @brief  停止闹钟输出并返回正常指示状态
 * @param  clock: 电子钟状态对象指针
 * @param  reason: 调试输出原因字符串
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_StopAlarm(ClockContext_t *clock, const char *reason, uint8_t *ui_dirty)
{
    if (clock->alarm_ringing == 0U)
    {
        return;
    }

    clock->alarm_ringing = 0U;
    BSP_Buzzer_Off();
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, reason);
}

/**
 * @brief  进入闹钟响铃状态
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
static void AppClockCore_StartAlarm(ClockContext_t *clock, uint8_t *ui_dirty)
{
    clock->alarm_ringing = 1U;
    clock->alarm_start_tick = Drv_Systick_Millis();
    clock->last_blink_tick = clock->alarm_start_tick;
    clock->blink_state = 1U;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, "alarm-start");
}

/**
 * @brief  初始化电子钟核心状态
 * @param  clock: 电子钟状态对象指针
 * @retval 无
 */
void AppClockCore_InitContext(ClockContext_t *clock)
{
    clock->alarm_time.hour = 6U;
    clock->alarm_time.minute = 30U;
    clock->alarm_time.second = 0U;
    clock->edit_time.hour = 12U;
    clock->edit_time.minute = 0U;
    clock->edit_time.second = 0U;
    clock->view = APP_VIEW_RUN;
    clock->rtc_source = DRV_RTC_SOURCE_SOFTWARE;
    clock->alarm_enabled = 0U;
    clock->alarm_ringing = 0U;
    clock->mute_enabled = 0U;
    clock->last_activity_tick = 0U;
    clock->alarm_start_tick = 0U;
    clock->last_alarm_second = 0xFFFFFFFFUL;
    clock->last_blink_tick = 0U;
    clock->blink_state = 0U;
}

/**
 * @brief  判断当前视图是否属于编辑态
 * @param  view: 当前应用视图
 * @retval 1 表示编辑态，0 表示运行态
 */
uint8_t AppClockCore_IsEditView(AppView_t view)
{
    return (uint8_t)(view != APP_VIEW_RUN);
}

/**
 * @brief  将时分秒结构转换为当天秒数
 * @param  time: 时间结构指针
 * @retval 对应的秒数
 */
uint32_t AppClockCore_TimeToSeconds(const DrvRtcTime_t *time)
{
    return (((uint32_t)time->hour * 3600UL) +
            ((uint32_t)time->minute * 60UL) +
            (uint32_t)time->second);
}

/**
 * @brief  获取视图枚举对应的调试名称
 * @param  view: 当前应用视图
 * @retval 视图名称字符串
 */
const char *AppClockCore_ViewName(AppView_t view)
{
    switch (view)
    {
        case APP_VIEW_RUN:
            return "RUN";

        case APP_VIEW_SET_TIME_HOUR:
            return "SET_TIME_HOUR";

        case APP_VIEW_SET_TIME_MINUTE:
            return "SET_TIME_MINUTE";

        case APP_VIEW_SET_TIME_SECOND:
            return "SET_TIME_SECOND";

        case APP_VIEW_SET_ALARM_HOUR:
            return "SET_ALARM_HOUR";

        case APP_VIEW_SET_ALARM_MINUTE:
            return "SET_ALARM_MINUTE";

        default:
            return "UNKNOWN";
    }
}

/**
 * @brief  获取 RTC 来源对应的详细名称
 * @param  source: RTC 来源枚举
 * @retval RTC 来源名称字符串
 */
const char *AppClockCore_RtcSourceName(DrvRtcSource_t source)
{
    switch (source)
    {
        case DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME:
            return "RTC-HW(DEFAULT)";

        case DRV_RTC_SOURCE_HARDWARE_BACKUP:
            return "RTC-HW(BACKUP)";

        case DRV_RTC_SOURCE_SOFTWARE:
        default:
            return "RTC-SOFT";
    }
}

/**
 * @brief  获取 RTC 来源对应的短标签
 * @param  source: RTC 来源枚举
 * @retval RTC 来源短标签字符串
 */
const char *AppClockCore_RtcSourceLabel(DrvRtcSource_t source)
{
    switch (source)
    {
        case DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME:
            return "DEFAULT";

        case DRV_RTC_SOURCE_HARDWARE_BACKUP:
            return "BACKUP";

        case DRV_RTC_SOURCE_SOFTWARE:
        default:
            return "SOFT";
    }
}

/**
 * @brief  获取当前模式显示文本
 * @param  clock: 电子钟状态对象指针
 * @retval 模式文本字符串
 */
const char *AppClockCore_ModeLabel(const ClockContext_t *clock)
{
    switch (clock->view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_TIME_SECOND:
            return "TIME SET";

        case APP_VIEW_SET_ALARM_HOUR:
        case APP_VIEW_SET_ALARM_MINUTE:
            return "ALARM SET";

        case APP_VIEW_RUN:
        default:
            if (clock->alarm_ringing != 0U)
            {
                return "RINGING";
            }
            return "RUN";
    }
}

/**
 * @brief  获取当前编辑字段名称
 * @param  clock: 电子钟状态对象指针
 * @retval 编辑字段文本字符串
 */
const char *AppClockCore_EditFieldLabel(const ClockContext_t *clock)
{
    switch (clock->view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_ALARM_HOUR:
            return "HOUR";

        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_ALARM_MINUTE:
            return "MIN";

        case APP_VIEW_SET_TIME_SECOND:
            return "SEC";

        default:
            return "";
    }
}

/**
 * @brief  打印当前电子钟状态到调试串口
 * @param  clock: 电子钟状态对象指针
 * @param  reason: 状态打印原因字符串
 * @retval 无
 */
void AppClockCore_PrintState(const ClockContext_t *clock, const char *reason)
{
    DrvRtcTime_t now;  /* 当前 RTC 时间快照 */

    Drv_Rtc_GetTime(&now);

    printf("[%s] now=%02u:%02u:%02u view=%s alarm=%s %02u:%02u ring=%u mute=%u edit=%02u:%02u:%02u\n",
           reason,
           now.hour,
           now.minute,
           now.second,
           AppClockCore_ViewName(clock->view),
           (clock->alarm_enabled != 0U) ? "ON" : "OFF",
           clock->alarm_time.hour,
           clock->alarm_time.minute,
           clock->alarm_ringing,
           clock->mute_enabled,
           clock->edit_time.hour,
           clock->edit_time.minute,
           clock->edit_time.second);
}

/**
 * @brief  切换闹钟使能状态
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @param  reason: 调试输出原因字符串
 * @retval 无
 */
void AppClockCore_ToggleAlarmEnable(ClockContext_t *clock,
                                    uint8_t *ui_dirty,
                                    uint8_t *settings_dirty,
                                    const char *reason)
{
    clock->alarm_enabled = (uint8_t)!clock->alarm_enabled;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_InvalidateSettings(settings_dirty);
    AppClockCore_PrintState(clock, reason);
}

/**
 * @brief  切换静音状态
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @param  reason: 调试输出原因字符串
 * @retval 无
 */
void AppClockCore_ToggleMute(ClockContext_t *clock,
                             uint8_t *ui_dirty,
                             uint8_t *settings_dirty,
                             const char *reason)
{
    clock->mute_enabled = (uint8_t)!clock->mute_enabled;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_InvalidateSettings(settings_dirty);
    AppClockCore_PrintState(clock, reason);
}

/**
 * @brief  处理物理按键事件并更新核心状态
 * @param  clock: 电子钟状态对象指针
 * @param  event: 按键事件
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
void AppClockCore_HandleKeyEvent(ClockContext_t *clock,
                                 KeyEvent_t event,
                                 uint8_t *ui_dirty,
                                 uint8_t *settings_dirty)
{
    if (event.type == KEY_EVENT_TYPE_NONE)
    {
        return;
    }

    if (clock->alarm_ringing != 0U)
    {
        AppClockCore_StopAlarm(clock, "alarm-stop-key", ui_dirty);
        return;
    }

    if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        if ((event.key_id == KEY_ID_KEY1) && (event.type == KEY_EVENT_TYPE_CLICK))
        {
            AppClockCore_AdvanceEditView(clock, ui_dirty, settings_dirty);
            return;
        }

        if ((event.key_id == KEY_ID_KEY1) && (event.type == KEY_EVENT_TYPE_LONG_PRESS))
        {
            AppClockCore_CancelEdit(clock, "cancel-edit", ui_dirty);
            return;
        }

        if ((event.key_id == KEY_ID_KEY2) && (event.type == KEY_EVENT_TYPE_CLICK))
        {
            AppClockCore_AdjustEditValue(clock, 1, ui_dirty);
            return;
        }

        if ((event.key_id == KEY_ID_KEY2) &&
            ((event.type == KEY_EVENT_TYPE_LONG_PRESS) || (event.type == KEY_EVENT_TYPE_REPEAT)))
        {
            AppClockCore_AdjustEditValue(clock, -1, ui_dirty);
        }
        return;
    }

    if ((event.key_id == KEY_ID_KEY1) && (event.type == KEY_EVENT_TYPE_CLICK))
    {
        AppClockCore_EnterTimeEdit(clock, ui_dirty);
        return;
    }

    if ((event.key_id == KEY_ID_KEY1) && (event.type == KEY_EVENT_TYPE_LONG_PRESS))
    {
        AppClockCore_ToggleAlarmEnable(clock, ui_dirty, settings_dirty, "alarm-toggle");
        return;
    }

    if ((event.key_id == KEY_ID_KEY2) && (event.type == KEY_EVENT_TYPE_LONG_PRESS))
    {
        AppClockCore_EnterAlarmEdit(clock, ui_dirty);
        return;
    }

    if ((event.key_id == KEY_ID_KEY2) && (event.type == KEY_EVENT_TYPE_CLICK))
    {
        AppClockCore_PrintState(clock, "run-click");
    }
}

/**
 * @brief  处理逻辑触摸按键命令
 * @param  clock: 电子钟状态对象指针
 * @param  button_id: 逻辑按钮编号
 * @param  is_hold_action: 是否为长按连发动作
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
void AppClockCore_HandleTouchCommand(ClockContext_t *clock,
                                     AppTouchButtonId_t button_id,
                                     uint8_t is_hold_action,
                                     uint8_t *ui_dirty,
                                     uint8_t *settings_dirty)
{
    if (clock->alarm_ringing != 0U)
    {
        AppClockCore_StopAlarm(clock, "alarm-stop-touch", ui_dirty);
        return;
    }

    if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        if (is_hold_action != 0U)
        {
            if (button_id == APP_TOUCH_BUTTON_3)
            {
                AppClockCore_AdjustEditValue(clock, 1, ui_dirty);
            }
            else if (button_id == APP_TOUCH_BUTTON_4)
            {
                AppClockCore_AdjustEditValue(clock, -1, ui_dirty);
            }
            return;
        }

        if (button_id == APP_TOUCH_BUTTON_1)
        {
            AppClockCore_AdvanceEditView(clock, ui_dirty, settings_dirty);
        }
        else if (button_id == APP_TOUCH_BUTTON_2)
        {
            AppClockCore_CancelEdit(clock, "touch-cancel", ui_dirty);
        }
        return;
    }

    if (is_hold_action != 0U)
    {
        return;
    }

    switch (button_id)
    {
        case APP_TOUCH_BUTTON_1:
            AppClockCore_EnterTimeEdit(clock, ui_dirty);
            break;

        case APP_TOUCH_BUTTON_2:
            AppClockCore_ToggleAlarmEnable(clock, ui_dirty, settings_dirty, "touch-alarm-toggle");
            break;

        case APP_TOUCH_BUTTON_3:
            AppClockCore_PrintState(clock, "touch-state");
            break;

        case APP_TOUCH_BUTTON_4:
            AppClockCore_EnterAlarmEdit(clock, ui_dirty);
            break;

        default:
            break;
    }
}

/**
 * @brief  检查编辑态是否超时退出
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
void AppClockCore_CheckEditTimeout(ClockContext_t *clock, uint8_t *ui_dirty)
{
    uint32_t now_tick;  /* 当前系统毫秒计数 */

    if (AppClockCore_IsEditView(clock->view) == 0U)
    {
        return;
    }

    now_tick = Drv_Systick_Millis();
    if ((now_tick - clock->last_activity_tick) >= APP_EDIT_TIMEOUT_MS)
    {
        AppClockCore_CancelEdit(clock, "edit-timeout", ui_dirty);
    }
}

/**
 * @brief  检查是否满足闹钟触发条件
 * @param  clock: 电子钟状态对象指针
 * @param  now: 当前时间指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
void AppClockCore_CheckAlarm(ClockContext_t *clock, const DrvRtcTime_t *now, uint8_t *ui_dirty)
{
    uint32_t now_seconds;  /* 当前时间对应的当天秒数 */

    if (clock->alarm_ringing != 0U)
    {
        if ((Drv_Systick_Millis() - clock->alarm_start_tick) >= APP_ALARM_RING_MS)
        {
            AppClockCore_StopAlarm(clock, "alarm-timeout", ui_dirty);
        }
        return;
    }

    if ((clock->alarm_enabled == 0U) || (AppClockCore_IsEditView(clock->view) != 0U))
    {
        return;
    }

    now_seconds = AppClockCore_TimeToSeconds(now);
    if (now_seconds == clock->last_alarm_second)
    {
        return;
    }

    clock->last_alarm_second = now_seconds;
    if ((now->hour == clock->alarm_time.hour) &&
        (now->minute == clock->alarm_time.minute) &&
        (now->second == 0U))
    {
        AppClockCore_StartAlarm(clock, ui_dirty);
    }
}

/**
 * @brief  根据当前状态更新 LED 与蜂鸣器输出
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
void AppClockCore_UpdateIndicators(ClockContext_t *clock, uint8_t *ui_dirty)
{
    uint8_t led_mask;  /* 当前准备输出的 LED 颜色掩码 */

    if (clock->alarm_ringing != 0U)
    {
        if (Drv_Systick_IsElapsed(&clock->last_blink_tick, APP_ALARM_BLINK_PERIOD_MS) != 0U)
        {
            clock->blink_state = (uint8_t)!clock->blink_state;
            AppClockCore_InvalidateUi(ui_dirty);
        }

        led_mask = (clock->blink_state != 0U) ? BSP_LED_RED_MASK : 0U;
        BSP_LED_SetMask(led_mask);
        BSP_Buzzer_SetState((uint8_t)((clock->blink_state != 0U) && (clock->mute_enabled == 0U)));
        return;
    }

    clock->blink_state = 0U;
    clock->last_blink_tick = Drv_Systick_Millis();
    BSP_Buzzer_Off();

    if ((clock->view == APP_VIEW_SET_TIME_HOUR) ||
        (clock->view == APP_VIEW_SET_TIME_MINUTE) ||
        (clock->view == APP_VIEW_SET_TIME_SECOND))
    {
        BSP_LED_SetMask(BSP_LED_BLUE_MASK);
        return;
    }

    if ((clock->view == APP_VIEW_SET_ALARM_HOUR) ||
        (clock->view == APP_VIEW_SET_ALARM_MINUTE))
    {
        BSP_LED_SetMask((uint8_t)(BSP_LED_GREEN_MASK | BSP_LED_BLUE_MASK));
        return;
    }

    led_mask = (clock->alarm_enabled != 0U) ? BSP_LED_GREEN_MASK : BSP_LED_BLUE_MASK;
    BSP_LED_SetMask(led_mask);
}

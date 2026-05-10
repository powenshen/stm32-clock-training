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
#define APP_ALARM_RING_MS            60000UL
#define APP_ALARM_BLINK_PERIOD_MS    200UL
#define APP_CHIME_DURATION_MS        300UL
#define APP_KEY_SOUND_DURATION_MS     80UL
#define APP_NUM_ENTRY_TIMEOUT_MS      2000UL

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
    clock->key_sound_active = 0U;
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
    clock->chime_active = 0U;
    clock->chime_start_tick = 0U;
    clock->last_chime_second = 0xFFFFFFFFUL;
    clock->key_sound_active = 0U;
    clock->key_sound_start_tick = 0U;
    clock->lcd_on = 1U;
    clock->num_entry_count = 0U;
    clock->num_entry_buffer = 0U;
    clock->num_entry_tick = 0U;
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
 * @brief  切换 LCD 显示开关
 * @param  clock: 电子钟状态对象指针
 * @param  ui_dirty: UI 刷新标志指针
 * @retval 无
 */
void AppClockCore_ToggleLcdPower(ClockContext_t *clock, uint8_t *ui_dirty)
{
    clock->lcd_on = (uint8_t)!clock->lcd_on;
    AppClockCore_InvalidateUi(ui_dirty);
    AppClockCore_PrintState(clock, clock->lcd_on != 0U ? "lcd-on" : "lcd-off");
}

/**
 * @brief  处理物理按键事件并更新核心状态
 * @param  clock: 电子钟状态对象指针
 * @param  event: 按键事件
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
/*
 * Hardware limitation: the board provides 2 physical keys (KEY1=PA0, KEY2=PC13).
 * The PPT requirement specifies 4 keys (KEY1=mode, KEY2=right-shift, KEY3=inc,
 * KEY4=dec). The 2-key mapping used here:
 *
 *   RUN mode:   KEY1 click = enter time edit,  KEY1 long = toggle alarm
 *               KEY2 click = debug print,       KEY2 long = enter alarm edit
 *   EDIT mode:  KEY1 click = next field,        KEY1 long = cancel edit
 *               KEY2 click = increment,          KEY2 long/repeat = decrement
 *
 * The 4-button touch UI and IR remote supplement the missing KEY3/KEY4 functions.
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

    AppClockCore_TriggerKeySound(clock);

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
        AppClockCore_ToggleMute(clock, ui_dirty, settings_dirty, "key-mute-toggle");
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
    AppClockCore_TriggerKeySound(clock);

    if (clock->alarm_ringing != 0U)
    {
        AppClockCore_StopAlarm(clock, "alarm-stop-touch", ui_dirty);
        return;
    }

    if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        if (is_hold_action != 0U)
        {
            if (button_id == APP_TOUCH_BUTTON_1)
            {
                AppClockCore_CancelEdit(clock, "touch-cancel", ui_dirty);
            }
            else if (button_id == APP_TOUCH_BUTTON_3)
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
            /* KEY1 模式键: 最后字段则保存返回，否则推进到下一字段 */
            if ((clock->view == APP_VIEW_SET_TIME_SECOND) ||
                (clock->view == APP_VIEW_SET_ALARM_MINUTE))
            {
                if (clock->view == APP_VIEW_SET_TIME_SECOND)
                {
                    AppClockCore_SaveTimeEdit(clock, ui_dirty);
                }
                else
                {
                    AppClockCore_SaveAlarmEdit(clock, ui_dirty, settings_dirty);
                }
            }
            else
            {
                AppClockCore_AdvanceEditView(clock, ui_dirty, settings_dirty);
            }
        }
        else if (button_id == APP_TOUCH_BUTTON_2)
        {
            /* KEY2 右移: 推进到下一字段 */
            AppClockCore_AdvanceEditView(clock, ui_dirty, settings_dirty);
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
            /* KEY1 模式键: 进入改时 */
            AppClockCore_EnterTimeEdit(clock, ui_dirty);
            break;

        case APP_TOUCH_BUTTON_2:
            /* KEY2 右移: 进入闹钟编辑 */
            AppClockCore_EnterAlarmEdit(clock, ui_dirty);
            break;

        case APP_TOUCH_BUTTON_3:
            /* KEY3 加: 切换闹钟使能 */
            AppClockCore_ToggleAlarmEnable(clock, ui_dirty, settings_dirty, "touch-alarm-toggle");
            break;

        case APP_TOUCH_BUTTON_4:
            /* KEY4 减: 切换静音 */
            AppClockCore_ToggleMute(clock, ui_dirty, settings_dirty, "touch-mute-toggle");
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

    if (AppClockCore_IsEditView(clock->view) != 0U)
    {
        now_tick = Drv_Systick_Millis();
        if ((now_tick - clock->last_activity_tick) >= APP_EDIT_TIMEOUT_MS)
        {
            AppClockCore_CancelEdit(clock, "edit-timeout", ui_dirty);
        }

        /* Number entry timeout: commit single digit after idle */
        if ((clock->num_entry_count == 1U) &&
            ((now_tick - clock->num_entry_tick) >= APP_NUM_ENTRY_TIMEOUT_MS))
        {
            uint8_t field_limit;
            uint8_t *field_ptr;

            field_limit = 24U;
            field_ptr = &clock->edit_time.hour;
            switch (clock->view)
            {
                case APP_VIEW_SET_TIME_HOUR:
                case APP_VIEW_SET_ALARM_HOUR:
                    field_limit = 24U;
                    field_ptr = &clock->edit_time.hour;
                    break;
                case APP_VIEW_SET_TIME_MINUTE:
                case APP_VIEW_SET_ALARM_MINUTE:
                    field_limit = 60U;
                    field_ptr = &clock->edit_time.minute;
                    break;
                case APP_VIEW_SET_TIME_SECOND:
                    field_limit = 60U;
                    field_ptr = &clock->edit_time.second;
                    break;
                default:
                    break;
            }
            if (field_ptr != 0)
            {
                *field_ptr = clock->num_entry_buffer % field_limit;
                clock->num_entry_count = 0U;
                AppClockCore_InvalidateUi(ui_dirty);
            }
        }
    }
    else
    {
        /* Not in edit view: reset number entry state */
        clock->num_entry_count = 0U;
    }
}

/**
 * @brief  处理红外遥控数字键输入
 * @param  clock: 电子钟状态对象指针
 * @param  digit: 输入的数字 (0~9)
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
void AppClockCore_HandleNumberEntry(ClockContext_t *clock,
                                    uint8_t digit,
                                    uint8_t *ui_dirty,
                                    uint8_t *settings_dirty)
{
    uint8_t field_limit;
    uint8_t *field_ptr;
    uint32_t now_tick;

    if ((digit > 9U) || (AppClockCore_IsEditView(clock->view) == 0U))
    {
        return;
    }

    switch (clock->view)
    {
        case APP_VIEW_SET_TIME_HOUR:
        case APP_VIEW_SET_ALARM_HOUR:
            field_limit = 24U;
            field_ptr = &clock->edit_time.hour;
            break;
        case APP_VIEW_SET_TIME_MINUTE:
        case APP_VIEW_SET_ALARM_MINUTE:
            field_limit = 60U;
            field_ptr = &clock->edit_time.minute;
            break;
        case APP_VIEW_SET_TIME_SECOND:
            field_limit = 60U;
            field_ptr = &clock->edit_time.second;
            break;
        default:
            return;
    }

    now_tick = Drv_Systick_Millis();

    if ((clock->num_entry_count > 0U) &&
        ((now_tick - clock->num_entry_tick) >= APP_NUM_ENTRY_TIMEOUT_MS))
    {
        *field_ptr = clock->num_entry_buffer % field_limit;
        clock->num_entry_count = 0U;
        AppClockCore_InvalidateUi(ui_dirty);
    }

    if (clock->num_entry_count == 0U)
    {
        clock->num_entry_buffer = digit;
        clock->num_entry_count = 1U;
        clock->num_entry_tick = now_tick;
        AppClockCore_InvalidateUi(ui_dirty);
        return;
    }

    {
        uint8_t value;
        value = (uint8_t)(clock->num_entry_buffer * 10U + digit);
        if (value >= field_limit)
        {
            value = (uint8_t)(value % field_limit);
        }
        *field_ptr = value;
        clock->num_entry_count = 0U;
        AppClockCore_RecordActivity(clock);
        AppClockCore_InvalidateUi(ui_dirty);
        AppClockCore_AdvanceEditView(clock, ui_dirty, settings_dirty);
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
 * @brief  触发按键音
 * @param  clock: 电子钟状态对象指针
 * @retval 无
 */
void AppClockCore_TriggerKeySound(ClockContext_t *clock)
{
    if (clock->mute_enabled != 0U)
    {
        return;
    }
    clock->key_sound_active = 1U;
    clock->key_sound_start_tick = Drv_Systick_Millis();
}

/**
 * @brief  检查整点提示条件
 * @param  clock: 电子钟状态对象指针
 * @param  now: 当前时间指针
 * @retval 无
 */
void AppClockCore_CheckHourlyChime(ClockContext_t *clock, const DrvRtcTime_t *now)
{
    uint32_t now_seconds;

    now_seconds = AppClockCore_TimeToSeconds(now);
    if ((now->minute == 0U) && (now->second == 0U) &&
        (clock->chime_active == 0U) && (now_seconds != clock->last_chime_second))
    {
        clock->chime_active = 1U;
        clock->chime_start_tick = Drv_Systick_Millis();
        clock->last_chime_second = now_seconds;
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
    uint8_t led_mask;   /* 当前准备输出的 LED 颜色掩码 */
    uint8_t buzzer_on;  /* 当前蜂鸣器输出状态 */

    buzzer_on = 0U;

    /* 1. Alarm ringing: blink LED + buzzer */
    if (clock->alarm_ringing != 0U)
    {
        if (Drv_Systick_IsElapsed(&clock->last_blink_tick, APP_ALARM_BLINK_PERIOD_MS) != 0U)
        {
            clock->blink_state = (uint8_t)!clock->blink_state;
            AppClockCore_InvalidateUi(ui_dirty);
        }

        led_mask = (clock->blink_state != 0U) ? BSP_LED_RED_MASK : 0U;
        buzzer_on = (uint8_t)((clock->blink_state != 0U) && (clock->mute_enabled == 0U));

        BSP_LED_SetMask(led_mask);
        BSP_Buzzer_SetState(buzzer_on);
        return;
    }

    clock->blink_state = 0U;
    clock->last_blink_tick = Drv_Systick_Millis();

    /* 2. Hourly chime: sustained beep for APP_CHIME_DURATION_MS */
    if (clock->chime_active != 0U)
    {
        if ((Drv_Systick_Millis() - clock->chime_start_tick) >= APP_CHIME_DURATION_MS)
        {
            clock->chime_active = 0U;
        }
        else
        {
            buzzer_on = 1U;
        }
    }
    /* 3. Key press sound: short beep for APP_KEY_SOUND_DURATION_MS */
    else if (clock->key_sound_active != 0U)
    {
        if ((Drv_Systick_Millis() - clock->key_sound_start_tick) >= APP_KEY_SOUND_DURATION_MS)
        {
            clock->key_sound_active = 0U;
        }
        else
        {
            buzzer_on = 1U;
        }
    }

    BSP_Buzzer_SetState(buzzer_on);

    /* LED control based on view */
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

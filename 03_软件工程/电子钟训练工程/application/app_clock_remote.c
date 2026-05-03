/*
 * Infrared command adapter.
 * This file keeps remote-specific key codes out of the core clock state machine.
 */
#include "app_clock_remote.h"

#include <stdio.h>

#include "app_clock_core.h"

#define IR_CMD_CH_MINUS        0x45U
#define IR_CMD_CH              0x46U
#define IR_CMD_CH_PLUS         0x47U
#define IR_CMD_PLAY_PAUSE      0x43U
#define IR_CMD_VOL_MINUS       0x07U
#define IR_CMD_VOL_PLUS        0x15U
#define IR_CMD_EQ              0x09U
#define IR_CMD_100_PLUS        0x19U

/**
 * @brief  将红外遥控事件映射为电子钟逻辑动作
 * @param  clock: 电子钟状态对象指针
 * @param  event: 红外遥控事件
 * @param  ui_dirty: UI 刷新标志指针
 * @param  settings_dirty: 参数保存标志指针
 * @retval 无
 */
void AppClockRemote_HandleEvent(ClockContext_t *clock,
                                DrvIrRemoteEvent_t event,
                                uint8_t *ui_dirty,
                                uint8_t *settings_dirty)
{
    if (event.type == DRV_IR_REMOTE_EVENT_NONE)
    {
        return;
    }

    switch (event.command)
    {
        case IR_CMD_CH:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_1, 0U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_EQ:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_2, 0U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_PLAY_PAUSE:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_3, 0U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_CH_PLUS:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_4, 0U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_VOL_PLUS:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_3, 1U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_VOL_MINUS:
            AppClockCore_HandleTouchCommand(clock, APP_TOUCH_BUTTON_4, 1U, ui_dirty, settings_dirty);
            break;

        case IR_CMD_CH_MINUS:
            AppClockCore_ToggleAlarmEnable(clock, ui_dirty, settings_dirty, "ir-alarm-toggle");
            break;

        case IR_CMD_100_PLUS:
            AppClockCore_ToggleMute(clock, ui_dirty, settings_dirty, "ir-mute-toggle");
            break;

        default:
            if (event.type == DRV_IR_REMOTE_EVENT_COMMAND)
            {
                printf("[ir-unknown] addr=0x%04X cmd=0x%02X\n", event.address, event.command);
            }
            break;
    }
}

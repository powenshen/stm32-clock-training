/* 按键驱动：
 * 1. 从具体引脚读取原始电平
 * 2. 每 1ms 做一次消抖和长按判定
 * 3. 向业务层上报“短按/长按”事件，而不掺杂业务规则
 */
#include "drv_key.h"

#include "board_config.h"

#define KEY_DEBOUNCE_MS        20U
#define KEY_LONG_PRESS_MS      800U

static volatile KeyEvent_t g_key_event = KEY_EVENT_NONE;

static uint8_t Drv_Key_ReadRaw(void)
{
    uint8_t pin_state;

    /* 读取的是板级定义中的按键引脚，再根据高低有效配置转换为统一逻辑值。 */
    pin_state = (uint8_t)GPIO_ReadInputDataBit(BOARD_KEY_PORT, BOARD_KEY_PIN);
#if BOARD_KEY_ACTIVE_LOW
    return (uint8_t)!pin_state;
#else
    return pin_state;
#endif
}

void Drv_Key_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    /* 当前默认把按键脚配置为上拉输入，对应常见“按下接地”的接法。 */
    RCC_APB2PeriphClockCmd(BOARD_KEY_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_KEY_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_KEY_PORT, &gpio_init_structure);
}

void Drv_Key_Task1ms(void)
{
    /* 用来做按键扫描、消除抖动、判定长按事件的状态机。
     * 这几个静态变量共同组成 1ms 调用一次的按键状态机：
     * sample_level：本次读取到的原始电平
     * stable_level：消抖后的稳定电平
     * stable_count：原始电平连续变化了多久
     * pressed_ms：稳定按下后已持续的时间
     */
    static uint8_t stable_level = 0U;
    static uint8_t sample_level = 0U;
    static uint16_t stable_count = 0U;
    static uint16_t pressed_ms = 0U;
    static uint8_t long_press_reported = 0U;

    sample_level = Drv_Key_ReadRaw();

    if (sample_level != stable_level)
    {
        stable_count++;
        if (stable_count >= KEY_DEBOUNCE_MS)
        {
            /* 只有电平连续变化足够久，才接受为新的稳定状态，
             * 这样可以滤掉机械按键抖动。
             */
            stable_level = sample_level;
            stable_count = 0U;

            if (stable_level == 0U)
            {
                /* 如果在长按阈值之前已经释放，就判定为一次短按事件。 */
                if (pressed_ms < KEY_LONG_PRESS_MS && long_press_reported == 0U)
                {
                    g_key_event = KEY_EVENT_CLICK;
                }
                pressed_ms = 0U;
                long_press_reported = 0U;
            }
        }
    }
    else
    {
        stable_count = 0U;
    }

    if (stable_level != 0U)
    {
        if (pressed_ms < 0xFFFFU)
        {
            pressed_ms++;
        }
        /* 长按事件只上报一次，之后保持计时直到按键释放。 */
        if ((pressed_ms >= KEY_LONG_PRESS_MS) && (long_press_reported == 0U))
        {
            g_key_event = KEY_EVENT_LONG_PRESS;
            long_press_reported = 1U;
        }
    }
}

KeyEvent_t Drv_Key_GetEvent(void)
{
    KeyEvent_t event;

    /* 事件被读取后立即清空，避免同一个事件被重复消费。 */
    event = g_key_event;
    g_key_event = KEY_EVENT_NONE;
    return event;
}

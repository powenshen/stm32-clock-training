#include "drv_key.h"

#include "board_config.h"

#define KEY_DEBOUNCE_MS        20U
#define KEY_LONG_PRESS_MS      800U

static volatile KeyEvent_t g_key_event = KEY_EVENT_NONE;

static uint8_t Drv_Key_ReadRaw(void)
{
    uint8_t pin_state;

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

    RCC_APB2PeriphClockCmd(BOARD_KEY_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_KEY_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_KEY_PORT, &gpio_init_structure);
}

void Drv_Key_Task1ms(void)
{
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
            stable_level = sample_level;
            stable_count = 0U;

            if (stable_level == 0U)
            {
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

    event = g_key_event;
    g_key_event = KEY_EVENT_NONE;
    return event;
}

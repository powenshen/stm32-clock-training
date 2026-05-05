#include "drv_key.h"

#include "board_config.h"
#include "sim_debug_config.h"

#define KEY_DEBOUNCE_MS        20U
#define KEY_LONG_PRESS_MS      800U
#define KEY_REPEAT_MS          200U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} KeyHardware_t;

typedef struct
{
    uint8_t stable_level;
    uint8_t sample_level;
    uint16_t stable_count;
    uint16_t pressed_ms;
    uint16_t repeat_ms;
    uint8_t long_press_reported;
} KeyRuntime_t;

static volatile KeyEvent_t g_key_event = {KEY_ID_NONE, KEY_EVENT_TYPE_NONE};

static const KeyHardware_t g_key_hardware[BOARD_KEY_COUNT] =
{
    {BOARD_KEY1_PORT, BOARD_KEY1_PIN},
    {BOARD_KEY2_PORT, BOARD_KEY2_PIN}
};

static KeyRuntime_t g_key_runtime[BOARD_KEY_COUNT];
static uint8_t g_key_sim_levels[BOARD_KEY_COUNT];

static int8_t Drv_Key_KeyIdToIndex(KeyId_t key_id)
{
    switch (key_id)
    {
        case KEY_ID_KEY1:
            return 0;

        case KEY_ID_KEY2:
            return 1;

        default:
            return -1;
    }
}

static uint8_t Drv_Key_ReadRaw(uint8_t key_index)
{
#if APP_CLOCK_SIM_ENABLED
    return g_key_sim_levels[key_index];
#else
    uint8_t pin_state;

    pin_state = (uint8_t)GPIO_ReadInputDataBit(g_key_hardware[key_index].port, g_key_hardware[key_index].pin);
#if BOARD_KEY_ACTIVE_LOW
    return (uint8_t)!pin_state;
#else
    return pin_state;
#endif
#endif
}

static void Drv_Key_PushEvent(KeyId_t key_id, KeyEventType_t type)
{
    if (g_key_event.type != KEY_EVENT_TYPE_NONE)
    {
        return;
    }

    g_key_event.key_id = key_id;
    g_key_event.type = type;
}

void Drv_Key_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    uint8_t key_index;

    g_key_event.key_id = KEY_ID_NONE;
    g_key_event.type = KEY_EVENT_TYPE_NONE;

    for (key_index = 0U; key_index < BOARD_KEY_COUNT; key_index++)
    {
        g_key_runtime[key_index].stable_level = 0U;
        g_key_runtime[key_index].sample_level = 0U;
        g_key_runtime[key_index].stable_count = 0U;
        g_key_runtime[key_index].pressed_ms = 0U;
        g_key_runtime[key_index].repeat_ms = 0U;
        g_key_runtime[key_index].long_press_reported = 0U;
        g_key_sim_levels[key_index] = 0U;
    }

#if APP_CLOCK_SIM_ENABLED
    return;
#endif

    RCC_APB2PeriphClockCmd(BOARD_KEY1_RCC | BOARD_KEY2_RCC, ENABLE);

    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;

    gpio_init_structure.GPIO_Pin = BOARD_KEY1_PIN;
    GPIO_Init(BOARD_KEY1_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BOARD_KEY2_PIN;
    GPIO_Init(BOARD_KEY2_PORT, &gpio_init_structure);
}

void Drv_Key_Task1ms(void)
{
    uint8_t key_index;

    for (key_index = 0U; key_index < BOARD_KEY_COUNT; key_index++)
    {
        KeyRuntime_t *runtime;
        uint8_t sample_level;

        runtime = &g_key_runtime[key_index];
        sample_level = Drv_Key_ReadRaw(key_index);

        if (sample_level != runtime->sample_level)
        {
            runtime->sample_level = sample_level;
            runtime->stable_count = 0U;
        }
        else if (runtime->stable_count < KEY_DEBOUNCE_MS)
        {
            runtime->stable_count++;
            if ((runtime->stable_count >= KEY_DEBOUNCE_MS) && (runtime->stable_level != runtime->sample_level))
            {
                runtime->stable_level = runtime->sample_level;

                if (runtime->stable_level == 0U)
                {
                    if ((runtime->pressed_ms < KEY_LONG_PRESS_MS) && (runtime->long_press_reported == 0U))
                    {
                        Drv_Key_PushEvent((KeyId_t)(KEY_ID_KEY1 + key_index), KEY_EVENT_TYPE_CLICK);
                    }

                    runtime->pressed_ms = 0U;
                    runtime->repeat_ms = 0U;
                    runtime->long_press_reported = 0U;
                }
            }
        }

        if (runtime->stable_level == 0U)
        {
            continue;
        }

        if (runtime->pressed_ms < 0xFFFFU)
        {
            runtime->pressed_ms++;
        }

        if (runtime->long_press_reported == 0U)
        {
            if (runtime->pressed_ms >= KEY_LONG_PRESS_MS)
            {
                Drv_Key_PushEvent((KeyId_t)(KEY_ID_KEY1 + key_index), KEY_EVENT_TYPE_LONG_PRESS);
                runtime->long_press_reported = 1U;
                runtime->repeat_ms = 0U;
            }
        }
        else
        {
            runtime->repeat_ms++;
            if (runtime->repeat_ms >= KEY_REPEAT_MS)
            {
                Drv_Key_PushEvent((KeyId_t)(KEY_ID_KEY1 + key_index), KEY_EVENT_TYPE_REPEAT);
                runtime->repeat_ms = 0U;
            }
        }
    }
}

KeyEvent_t Drv_Key_GetEvent(void)
{
    KeyEvent_t event;

    __disable_irq();
    event = g_key_event;
    g_key_event.key_id = KEY_ID_NONE;
    g_key_event.type = KEY_EVENT_TYPE_NONE;
    __enable_irq();

    return event;
}

void Drv_Key_SimSetLevel(KeyId_t key_id, uint8_t is_pressed)
{
    int8_t key_index;

    key_index = Drv_Key_KeyIdToIndex(key_id);
    if (key_index < 0)
    {
        return;
    }

    g_key_sim_levels[(uint8_t)key_index] = (uint8_t)(is_pressed != 0U);
}

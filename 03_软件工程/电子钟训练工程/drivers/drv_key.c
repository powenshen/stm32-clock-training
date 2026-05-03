#include "drv_key.h"

#include "board_config.h"

#define KEY_DEBOUNCE_MS        20U
#define KEY_LONG_PRESS_MS      800U
#define KEY_REPEAT_MS          200U

typedef struct
{
    GPIO_TypeDef *port;  /* 按键所在 GPIO 端口 */
    uint16_t pin;        /* 按键对应 GPIO 引脚 */
} KeyHardware_t;

typedef struct
{
    uint8_t stable_level;          /* 当前稳定按键电平 */
    uint8_t sample_level;          /* 最近一次采样到的按键电平 */
    uint16_t stable_count;         /* 当前采样电平已持续的毫秒数 */
    uint16_t pressed_ms;           /* 按下持续时间计数 */
    uint16_t repeat_ms;            /* 长按连发间隔计数 */
    uint8_t long_press_reported;   /* 长按事件是否已经上报 */
} KeyRuntime_t;

static volatile KeyEvent_t g_key_event = {KEY_ID_NONE, KEY_EVENT_TYPE_NONE};  /* 待主循环读取的按键事件缓存 */

static const KeyHardware_t g_key_hardware[BOARD_KEY_COUNT] =  /* 板级按键硬件映射表 */
{
    {BOARD_KEY1_PORT, BOARD_KEY1_PIN},
    {BOARD_KEY2_PORT, BOARD_KEY2_PIN}
};

static KeyRuntime_t g_key_runtime[BOARD_KEY_COUNT];  /* 每个按键各自的扫描运行时状态 */

/**
 * @brief  读取指定按键的原始电平状态
 * @param  key_index: 按键索引
 * @retval 1 表示按下，0 表示松开
 */
static uint8_t Drv_Key_ReadRaw(uint8_t key_index)
{
    uint8_t pin_state;  /* GPIO 实际读回的引脚电平 */

    pin_state = (uint8_t)GPIO_ReadInputDataBit(g_key_hardware[key_index].port, g_key_hardware[key_index].pin);
#if BOARD_KEY_ACTIVE_LOW
    return (uint8_t)!pin_state;
#else
    return pin_state;
#endif
}

/**
 * @brief  压入一条按键事件
 * @param  key_id: 按键编号
 * @param  type: 按键事件类型
 * @retval 无
 */
static void Drv_Key_PushEvent(KeyId_t key_id, KeyEventType_t type)
{
    if (g_key_event.type != KEY_EVENT_TYPE_NONE)
    {
        return;
    }

    g_key_event.key_id = key_id;
    g_key_event.type = type;
}

/**
 * @brief  初始化按键驱动
 * @param  无
 * @retval 无
 */
void Drv_Key_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;  /* 按键 GPIO 初始化参数 */

    RCC_APB2PeriphClockCmd(BOARD_KEY1_RCC | BOARD_KEY2_RCC, ENABLE);

    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;

    gpio_init_structure.GPIO_Pin = BOARD_KEY1_PIN;
    GPIO_Init(BOARD_KEY1_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BOARD_KEY2_PIN;
    GPIO_Init(BOARD_KEY2_PORT, &gpio_init_structure);
}

/**
 * @brief  按键 1ms 周期扫描任务
 * @param  无
 * @retval 无
 */
void Drv_Key_Task1ms(void)
{
    uint8_t key_index;  /* 当前扫描的按键索引 */

    for (key_index = 0U; key_index < BOARD_KEY_COUNT; key_index++)
    {
        KeyRuntime_t *runtime;  /* 当前按键的运行时状态对象 */
        uint8_t sample_level;   /* 本次扫描读到的原始电平 */

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

/**
 * @brief  读取一条按键事件并清空事件缓存
 * @param  无
 * @retval 按键事件结构
 */
KeyEvent_t Drv_Key_GetEvent(void)
{
    KeyEvent_t event;  /* 准备返回给上层的按键事件 */

    __disable_irq();
    event = g_key_event;
    g_key_event.key_id = KEY_ID_NONE;
    g_key_event.type = KEY_EVENT_TYPE_NONE;
    __enable_irq();

    return event;
}

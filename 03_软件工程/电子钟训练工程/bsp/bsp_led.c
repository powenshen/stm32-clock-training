#include "bsp_led.h"

#include "board_config.h"

#define BSP_LED_ALL_MASK    (BSP_LED_RED_MASK | BSP_LED_GREEN_MASK | BSP_LED_BLUE_MASK)

static uint8_t g_led_mask = 0U;

static void BSP_LED_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t is_on)
{
#if BOARD_LED_ACTIVE_LOW
    GPIO_WriteBit(port, pin, (BitAction)!is_on);
#else
    GPIO_WriteBit(port, pin, (BitAction)is_on);
#endif
}

void BSP_LED_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    g_led_mask = 0U;

    RCC_APB2PeriphClockCmd(BOARD_LED_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_LED_RED_PIN | BOARD_LED_GREEN_PIN | BOARD_LED_BLUE_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_LED_RED_PORT, &gpio_init_structure);

    BSP_LED_AllOff();
}

void BSP_LED_SetMask(uint8_t mask)
{
    g_led_mask = (uint8_t)(mask & BSP_LED_ALL_MASK);

    BSP_LED_WritePin(BOARD_LED_RED_PORT, BOARD_LED_RED_PIN, (uint8_t)((g_led_mask & BSP_LED_RED_MASK) != 0U));
    BSP_LED_WritePin(BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_PIN, (uint8_t)((g_led_mask & BSP_LED_GREEN_MASK) != 0U));
    BSP_LED_WritePin(BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_PIN, (uint8_t)((g_led_mask & BSP_LED_BLUE_MASK) != 0U));
}

void BSP_LED_ToggleMask(uint8_t mask)
{
    BSP_LED_SetMask((uint8_t)(g_led_mask ^ (mask & BSP_LED_ALL_MASK)));
}

uint8_t BSP_LED_GetMask(void)
{
    return g_led_mask;
}

void BSP_LED_AllOff(void)
{
    BSP_LED_SetMask(0U);
}

void BSP_LED_On(void)
{
    BSP_LED_SetMask(BSP_LED_GREEN_MASK);
}

void BSP_LED_Off(void)
{
    BSP_LED_AllOff();
}

void BSP_LED_Toggle(void)
{
    BSP_LED_ToggleMask(BSP_LED_GREEN_MASK);
}

#include "bsp_led.h"

#include "board_config.h"

static void BSP_LED_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t is_on)
{
#if BOARD_LED_ACTIVE_LOW
    GPIO_WriteBit(port, pin, (BitAction)!is_on);
#else
    GPIO_WriteBit(port, pin, (BitAction)is_on);
#endif
}

static uint8_t BSP_LED_IsPinOn(GPIO_TypeDef *port, uint16_t pin)
{
    uint8_t pin_level;

    pin_level = (uint8_t)GPIO_ReadOutputDataBit(port, pin);
#if BOARD_LED_ACTIVE_LOW
    return (uint8_t)!pin_level;
#else
    return pin_level;
#endif
}

void BSP_LED_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    RCC_APB2PeriphClockCmd(BOARD_LED_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_LED_RED_PIN | BOARD_LED_GREEN_PIN | BOARD_LED_BLUE_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_LED_RED_PORT, &gpio_init_structure);

    BSP_LED_AllOff();
}

void BSP_LED_SetMask(uint8_t mask)
{
    BSP_LED_WritePin(BOARD_LED_RED_PORT, BOARD_LED_RED_PIN, (uint8_t)((mask & BSP_LED_RED_MASK) != 0U));
    BSP_LED_WritePin(BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_PIN, (uint8_t)((mask & BSP_LED_GREEN_MASK) != 0U));
    BSP_LED_WritePin(BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_PIN, (uint8_t)((mask & BSP_LED_BLUE_MASK) != 0U));
}

void BSP_LED_ToggleMask(uint8_t mask)
{
    if ((mask & BSP_LED_RED_MASK) != 0U)
    {
        BSP_LED_WritePin(BOARD_LED_RED_PORT, BOARD_LED_RED_PIN, (uint8_t)!BSP_LED_IsPinOn(BOARD_LED_RED_PORT, BOARD_LED_RED_PIN));
    }

    if ((mask & BSP_LED_GREEN_MASK) != 0U)
    {
        BSP_LED_WritePin(BOARD_LED_GREEN_PORT,
                         BOARD_LED_GREEN_PIN,
                         (uint8_t)!BSP_LED_IsPinOn(BOARD_LED_GREEN_PORT, BOARD_LED_GREEN_PIN));
    }

    if ((mask & BSP_LED_BLUE_MASK) != 0U)
    {
        BSP_LED_WritePin(BOARD_LED_BLUE_PORT,
                         BOARD_LED_BLUE_PIN,
                         (uint8_t)!BSP_LED_IsPinOn(BOARD_LED_BLUE_PORT, BOARD_LED_BLUE_PIN));
    }
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

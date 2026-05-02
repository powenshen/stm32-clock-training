#include "bsp_led.h"

#include "board_config.h"

static void BSP_LED_Write(BitAction state)
{
#if BOARD_LED_ACTIVE_LOW
    GPIO_WriteBit(BOARD_LED_PORT, BOARD_LED_PIN, (BitAction)!state);
#else
    GPIO_WriteBit(BOARD_LED_PORT, BOARD_LED_PIN, state);
#endif
}

void BSP_LED_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    RCC_APB2PeriphClockCmd(BOARD_LED_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_LED_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_LED_PORT, &gpio_init_structure);

    BSP_LED_Off();
}

void BSP_LED_On(void)
{
    BSP_LED_Write(Bit_SET);
}

void BSP_LED_Off(void)
{
    BSP_LED_Write(Bit_RESET);
}

void BSP_LED_Toggle(void)
{
    BitAction next_state;

    next_state = (BitAction)(1U - GPIO_ReadOutputDataBit(BOARD_LED_PORT, BOARD_LED_PIN));
#if BOARD_LED_ACTIVE_LOW
    next_state = (BitAction)!next_state;
#endif
    BSP_LED_Write(next_state);
}

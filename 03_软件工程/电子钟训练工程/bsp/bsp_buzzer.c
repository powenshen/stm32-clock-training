#include "bsp_buzzer.h"

#include "board_config.h"

static void BSP_Buzzer_Write(uint8_t is_on)
{
#if BOARD_BUZZER_ACTIVE_LOW
    GPIO_WriteBit(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, (BitAction)!is_on);
#else
    GPIO_WriteBit(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, (BitAction)is_on);
#endif
}

static uint8_t BSP_Buzzer_ReadState(void)
{
    uint8_t pin_level;

    pin_level = (uint8_t)GPIO_ReadOutputDataBit(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN);
#if BOARD_BUZZER_ACTIVE_LOW
    return (uint8_t)!pin_level;
#else
    return pin_level;
#endif
}

void BSP_Buzzer_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    RCC_APB2PeriphClockCmd(BOARD_BUZZER_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_BUZZER_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_BUZZER_PORT, &gpio_init_structure);

    BSP_Buzzer_Off();
}

void BSP_Buzzer_SetState(uint8_t is_on)
{
    BSP_Buzzer_Write((uint8_t)(is_on != 0U));
}

void BSP_Buzzer_On(void)
{
    BSP_Buzzer_SetState(1U);
}

void BSP_Buzzer_Off(void)
{
    BSP_Buzzer_SetState(0U);
}

void BSP_Buzzer_Toggle(void)
{
    BSP_Buzzer_SetState((uint8_t)!BSP_Buzzer_ReadState());
}

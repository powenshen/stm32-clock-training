#include "bsp_buzzer.h"

#include "board_config.h"

static uint8_t g_buzzer_state = 0U;

static void BSP_Buzzer_Write(uint8_t is_on)
{
#if BOARD_BUZZER_ACTIVE_LOW
    GPIO_WriteBit(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, (BitAction)!is_on);
#else
    GPIO_WriteBit(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, (BitAction)is_on);
#endif
}

void BSP_Buzzer_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    g_buzzer_state = 0U;

    RCC_APB2PeriphClockCmd(BOARD_BUZZER_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_BUZZER_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_BUZZER_PORT, &gpio_init_structure);

    BSP_Buzzer_Off();
}

void BSP_Buzzer_SetState(uint8_t is_on)
{
    g_buzzer_state = (uint8_t)(is_on != 0U);

    BSP_Buzzer_Write(g_buzzer_state);
}

uint8_t BSP_Buzzer_GetState(void)
{
    return g_buzzer_state;
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
    BSP_Buzzer_SetState((uint8_t)!g_buzzer_state);
}

#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32f10x.h"

typedef enum
{
    BSP_LED_RED_MASK = 0x01U,
    BSP_LED_GREEN_MASK = 0x02U,
    BSP_LED_BLUE_MASK = 0x04U
} BspLedMask_t;

void BSP_LED_Init(void);
void BSP_LED_SetMask(uint8_t mask);
void BSP_LED_ToggleMask(uint8_t mask);
uint8_t BSP_LED_GetMask(void);
void BSP_LED_AllOff(void);
void BSP_LED_On(void);
void BSP_LED_Off(void);
void BSP_LED_Toggle(void);

#endif

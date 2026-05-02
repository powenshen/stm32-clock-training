#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include "stm32f10x.h"

void BSP_Buzzer_Init(void);
void BSP_Buzzer_SetState(uint8_t is_on);
void BSP_Buzzer_On(void);
void BSP_Buzzer_Off(void);
void BSP_Buzzer_Toggle(void);

#endif

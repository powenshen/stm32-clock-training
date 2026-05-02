#ifndef DRV_SYSTICK_H
#define DRV_SYSTICK_H

#include "stm32f10x.h"

void Drv_Systick_Init(void);
void Drv_Systick_IrqHandler(void);
uint32_t Drv_Systick_Millis(void);
uint8_t Drv_Systick_IsElapsed(uint32_t *last_tick, uint32_t period_ms);

#endif

#ifndef DRV_DEBUG_UART_H
#define DRV_DEBUG_UART_H

#include "stm32f10x.h"

void Drv_DebugUart_Init(void);
void Drv_DebugUart_SendChar(char ch);
void Drv_DebugUart_SendString(const char *text);

#endif

#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include "stm32f10x.h"

void App_Clock_Init(void);
void App_Clock_Task(void);
void App_Clock_Task1ms(void);

#endif

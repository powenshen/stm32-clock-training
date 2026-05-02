#ifndef DRV_KEY_H
#define DRV_KEY_H

#include "stm32f10x.h"

typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_CLICK,
    KEY_EVENT_LONG_PRESS
} KeyEvent_t;

void Drv_Key_Init(void);
void Drv_Key_Task1ms(void);
KeyEvent_t Drv_Key_GetEvent(void);

#endif

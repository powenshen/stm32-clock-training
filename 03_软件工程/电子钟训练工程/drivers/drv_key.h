#ifndef DRV_KEY_H
#define DRV_KEY_H

#include "stm32f10x.h"

typedef enum
{
    KEY_ID_NONE = 0,
    KEY_ID_KEY1,
    KEY_ID_KEY2
} KeyId_t;

typedef enum
{
    KEY_EVENT_TYPE_NONE = 0,
    KEY_EVENT_TYPE_CLICK,
    KEY_EVENT_TYPE_LONG_PRESS,
    KEY_EVENT_TYPE_REPEAT
} KeyEventType_t;

typedef struct
{
    KeyId_t key_id;
    KeyEventType_t type;
} KeyEvent_t;

void Drv_Key_Init(void);
void Drv_Key_Task1ms(void);
KeyEvent_t Drv_Key_GetEvent(void);

#endif

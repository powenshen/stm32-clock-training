#ifndef BSP_TOUCH_H
#define BSP_TOUCH_H

#include "stm32f10x.h"

typedef struct
{
    uint16_t x;
    uint16_t y;
} BspTouchPoint_t;

void BSP_Touch_Init(void);
uint8_t BSP_Touch_GetPoint(BspTouchPoint_t *point,
                           uint8_t scan_mode,
                           uint16_t lcd_width,
                           uint16_t lcd_height);

#endif

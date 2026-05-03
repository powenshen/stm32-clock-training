/*
 * NEC infrared remote receiver driver.
 * The decoder is edge-timestamp based and produces command/repeat events for the application layer.
 */
#ifndef DRV_IR_REMOTE_H
#define DRV_IR_REMOTE_H

#include "stm32f10x.h"

typedef enum
{
    DRV_IR_REMOTE_EVENT_NONE = 0,
    DRV_IR_REMOTE_EVENT_COMMAND,
    DRV_IR_REMOTE_EVENT_REPEAT
} DrvIrRemoteEventType_t;

typedef struct
{
    uint16_t address;
    uint8_t command;
    DrvIrRemoteEventType_t type;
} DrvIrRemoteEvent_t;

void Drv_IrRemote_Init(void);
void Drv_IrRemote_IrqHandler(void);
DrvIrRemoteEvent_t Drv_IrRemote_GetEvent(void);

#endif

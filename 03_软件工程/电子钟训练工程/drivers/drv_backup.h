/*
 * Simple backup-register access wrapper.
 * The application uses this driver to persist small settings in the RTC backup domain.
 */
#ifndef DRV_BACKUP_H
#define DRV_BACKUP_H

#include "stm32f10x.h"

void Drv_Backup_Init(void);
uint16_t Drv_Backup_Read(uint8_t register_index);
void Drv_Backup_Write(uint8_t register_index, uint16_t value);

#endif

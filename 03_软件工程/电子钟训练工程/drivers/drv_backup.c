/*
 * Backup-register storage helper for application parameters.
 * This file intentionally exposes only word-level read/write primitives.
 */
#include "drv_backup.h"

#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"

static const uint16_t g_backup_registers[] =
{
    BKP_DR1, BKP_DR2, BKP_DR3, BKP_DR4, BKP_DR5,
    BKP_DR6, BKP_DR7, BKP_DR8, BKP_DR9, BKP_DR10
};

static uint8_t Drv_Backup_IsIndexValid(uint8_t register_index)
{
    return (uint8_t)((register_index > 0U) &&
                     (register_index <= (uint8_t)(sizeof(g_backup_registers) / sizeof(g_backup_registers[0]))));
}

void Drv_Backup_Init(void)
{
    /* The backup domain is shared with RTC, so only enable access here. */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

uint16_t Drv_Backup_Read(uint8_t register_index)
{
    if (Drv_Backup_IsIndexValid(register_index) == 0U)
    {
        return 0U;
    }

    return BKP_ReadBackupRegister(g_backup_registers[register_index - 1U]);
}

void Drv_Backup_Write(uint8_t register_index, uint16_t value)
{
    if (Drv_Backup_IsIndexValid(register_index) == 0U)
    {
        return;
    }

    BKP_WriteBackupRegister(g_backup_registers[register_index - 1U], value);
}

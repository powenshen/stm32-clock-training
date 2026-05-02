#ifndef DRV_RTC_H
#define DRV_RTC_H

#include "stm32f10x.h"

typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DrvRtcTime_t;

typedef enum
{
    DRV_RTC_SOURCE_SOFTWARE = 0,
    DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME,
    DRV_RTC_SOURCE_HARDWARE_BACKUP
} DrvRtcSource_t;

DrvRtcSource_t Drv_Rtc_Init(const DrvRtcTime_t *default_time);
void Drv_Rtc_Task1ms(void);
void Drv_Rtc_GetTime(DrvRtcTime_t *time);
void Drv_Rtc_SetTime(const DrvRtcTime_t *time);
uint8_t Drv_Rtc_IsHardwareActive(void);

#endif

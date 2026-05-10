#ifndef DRV_RTC_H
#define DRV_RTC_H

#include "stm32f10x.h"

typedef struct
{
    uint8_t hour;    /* 小时，范围 0~23 */
    uint8_t minute;  /* 分钟，范围 0~59 */
    uint8_t second;  /* 秒，范围 0~59 */
} DrvRtcTime_t;

typedef struct
{
    uint16_t year;   /* 年份，例如 2026 */
    uint8_t month;   /* 月份，1~12 */
    uint8_t day;     /* 日期，1~31 */
} DrvRtcDate_t;

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
void Drv_Rtc_GetDate(DrvRtcDate_t *date);
void Drv_Rtc_SetDate(const DrvRtcDate_t *date);
uint8_t Drv_Rtc_GetDayOfWeek(const DrvRtcDate_t *date);
uint8_t Drv_Rtc_WasBackupLost(void);

#endif

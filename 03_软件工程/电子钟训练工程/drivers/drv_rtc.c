#include "drv_rtc.h"

#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"

#define RTC_BACKUP_MARKER      0x5A5AU
#define RTC_SECONDS_PER_DAY    86400UL
#define RTC_LSE_READY_TIMEOUT  0x000FFFFFUL

static uint8_t g_rtc_hardware_active = 0U;
static uint32_t g_software_seconds = 0UL;
static uint16_t g_software_ms = 0U;

static uint8_t Drv_Rtc_IsTimeValid(const DrvRtcTime_t *time)
{
    if (time == 0)
    {
        return 0U;
    }

    if (time->hour >= 24U)
    {
        return 0U;
    }

    if (time->minute >= 60U)
    {
        return 0U;
    }

    if (time->second >= 60U)
    {
        return 0U;
    }

    return 1U;
}

static uint32_t Drv_Rtc_TimeToSeconds(const DrvRtcTime_t *time)
{
    return (((uint32_t)time->hour * 3600UL) +
            ((uint32_t)time->minute * 60UL) +
            (uint32_t)time->second);
}

static void Drv_Rtc_SecondsToTime(uint32_t seconds, DrvRtcTime_t *time)
{
    seconds %= RTC_SECONDS_PER_DAY;

    time->hour = (uint8_t)(seconds / 3600UL);
    seconds %= 3600UL;
    time->minute = (uint8_t)(seconds / 60UL);
    time->second = (uint8_t)(seconds % 60UL);
}

static void Drv_Rtc_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t Drv_Rtc_WaitLseReady(void)
{
    uint32_t timeout;

    timeout = RTC_LSE_READY_TIMEOUT;
    while ((RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) && (timeout > 0UL))
    {
        timeout--;
    }

    return (uint8_t)(timeout > 0UL);
}

static uint8_t Drv_Rtc_InitHardware(uint32_t default_seconds, uint8_t *used_backup_time)
{
    uint16_t marker;

    Drv_Rtc_EnableBackupAccess();
    marker = BKP_ReadBackupRegister(BKP_DR1);

    if (marker != RTC_BACKUP_MARKER)
    {
        RCC_BackupResetCmd(ENABLE);
        RCC_BackupResetCmd(DISABLE);

        RCC_LSEConfig(RCC_LSE_ON);
        if (Drv_Rtc_WaitLseReady() == 0U)
        {
            return 0U;
        }

        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        RCC_RTCCLKCmd(ENABLE);

        RTC_WaitForSynchro();
        RTC_WaitForLastTask();
        RTC_SetPrescaler(32767U);
        RTC_WaitForLastTask();
        RTC_SetCounter(default_seconds);
        RTC_WaitForLastTask();

        BKP_WriteBackupRegister(BKP_DR1, RTC_BACKUP_MARKER);
        *used_backup_time = 0U;
        return 1U;
    }

    RCC_LSEConfig(RCC_LSE_ON);
    if (Drv_Rtc_WaitLseReady() == 0U)
    {
        return 0U;
    }

    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_WaitForLastTask();

    *used_backup_time = 1U;
    return 1U;
}

DrvRtcSource_t Drv_Rtc_Init(const DrvRtcTime_t *default_time)
{
    DrvRtcTime_t initial_time;
    uint8_t used_backup_time;

    if (Drv_Rtc_IsTimeValid(default_time) != 0U)
    {
        initial_time = *default_time;
    }
    else
    {
        initial_time.hour = 12U;
        initial_time.minute = 0U;
        initial_time.second = 0U;
    }

    used_backup_time = 0U;
    if (Drv_Rtc_InitHardware(Drv_Rtc_TimeToSeconds(&initial_time), &used_backup_time) != 0U)
    {
        g_rtc_hardware_active = 1U;
        return (used_backup_time != 0U) ? DRV_RTC_SOURCE_HARDWARE_BACKUP : DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME;
    }

    g_rtc_hardware_active = 0U;
    g_software_seconds = Drv_Rtc_TimeToSeconds(&initial_time);
    g_software_ms = 0U;
    return DRV_RTC_SOURCE_SOFTWARE;
}

void Drv_Rtc_Task1ms(void)
{
    if (g_rtc_hardware_active != 0U)
    {
        return;
    }

    g_software_ms++;
    if (g_software_ms >= 1000U)
    {
        g_software_ms = 0U;
        g_software_seconds = (g_software_seconds + 1UL) % RTC_SECONDS_PER_DAY;
    }
}

void Drv_Rtc_GetTime(DrvRtcTime_t *time)
{
    uint32_t seconds;

    if (time == 0)
    {
        return;
    }

    if (g_rtc_hardware_active != 0U)
    {
        seconds = RTC_GetCounter();
    }
    else
    {
        seconds = g_software_seconds;
    }

    Drv_Rtc_SecondsToTime(seconds, time);
}

void Drv_Rtc_SetTime(const DrvRtcTime_t *time)
{
    uint32_t seconds;

    if (Drv_Rtc_IsTimeValid(time) == 0U)
    {
        return;
    }

    seconds = Drv_Rtc_TimeToSeconds(time);

    if (g_rtc_hardware_active != 0U)
    {
        RTC_WaitForLastTask();
        RTC_SetCounter(seconds);
        RTC_WaitForLastTask();
        return;
    }

    g_software_seconds = seconds;
    g_software_ms = 0U;
}

uint8_t Drv_Rtc_IsHardwareActive(void)
{
    return g_rtc_hardware_active;
}

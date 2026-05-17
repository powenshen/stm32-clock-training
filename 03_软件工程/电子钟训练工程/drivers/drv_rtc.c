#include "drv_rtc.h"

#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"

#define RTC_BACKUP_MARKER      0x5A5AU
#define RTC_SECONDS_PER_DAY    86400UL
#define RTC_LSE_READY_TIMEOUT  0x000FFFFFUL

static uint8_t g_rtc_hardware_active = 0U;  /* 当前是否启用硬件 RTC */
static uint32_t g_software_seconds = 0UL;   /* 软件 RTC 的当天秒计数 */
static uint16_t g_software_ms = 0U;         /* 软件 RTC 的毫秒累加计数 */
static DrvRtcDate_t g_software_date;        /* 软件 RTC 的日期 */
static uint8_t g_rtc_backup_was_lost = 0U;  /* 后备电池失效标志 */

/**
 * @brief  检查时间结构是否有效
 * @param  time: 时间结构指针
 * @retval 1 表示有效，0 表示无效
 */
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

/**
 * @brief  将时分秒结构转换为当天秒数
 * @param  time: 时间结构指针
 * @retval 对应的秒数
 */
static uint32_t Drv_Rtc_TimeToSeconds(const DrvRtcTime_t *time)
{
    return (((uint32_t)time->hour * 3600UL) +
            ((uint32_t)time->minute * 60UL) +
            (uint32_t)time->second);
}

/**
 * @brief  将当天秒数转换为时分秒结构
 * @param  seconds: 当天秒数
 * @param  time: 时间结构输出指针
 * @retval 无
 */
static void Drv_Rtc_SecondsToTime(uint32_t seconds, DrvRtcTime_t *time)
{
    seconds %= RTC_SECONDS_PER_DAY;

    time->hour = (uint8_t)(seconds / 3600UL);
    seconds %= 3600UL;
    time->minute = (uint8_t)(seconds / 60UL);
    time->second = (uint8_t)(seconds % 60UL);
}

static uint8_t Drv_Rtc_IsLeapYear(uint16_t year)
{
    return (uint8_t)(((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U));
}

static uint8_t Drv_Rtc_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    uint8_t d;

    d = days[month - 1U];
    if ((month == 2U) && (Drv_Rtc_IsLeapYear(year) != 0U))
    {
        d = 29U;
    }
    return d;
}

static void Drv_Rtc_AdvanceDate(DrvRtcDate_t *date)
{
    date->day++;
    if (date->day > Drv_Rtc_DaysInMonth(date->year, date->month))
    {
        date->day = 1U;
        date->month++;
        if (date->month > 12U)
        {
            date->month = 1U;
            date->year++;
        }
    }
}

/**
 * @brief  使能后备域访问权限
 * @param  无
 * @retval 无
 */
static void Drv_Rtc_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

/**
 * @brief  等待 LSE 晶振稳定
 * @param  无
 * @retval 1 表示等待成功，0 表示等待超时
 */
static uint8_t Drv_Rtc_WaitLseReady(void)
{
    uint32_t timeout;  /* LSE 就绪等待超时计数器 */

    timeout = RTC_LSE_READY_TIMEOUT;
    while ((RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) && (timeout > 0UL))
    {
        timeout--;
    }

    return (uint8_t)(timeout > 0UL);
}

/**
 * @brief  初始化硬件 RTC
 * @param  default_seconds: 默认时间对应的秒数
 * @param  used_backup_time: 是否使用了后备域已有时间的输出标志
 * @retval 1 表示初始化成功，0 表示初始化失败
 */
static uint8_t Drv_Rtc_InitHardware(uint32_t default_seconds, uint8_t *used_backup_time)
{
    uint16_t marker;  /* 后备域初始化标记值 */

    Drv_Rtc_EnableBackupAccess();
    marker = BKP_ReadBackupRegister(BKP_DR1);

    if (marker != RTC_BACKUP_MARKER)
    {
        /* Check secondary marker: if previously configured, battery likely failed */
        if (BKP_ReadBackupRegister(BKP_DR10) == RTC_BACKUP_MARKER)
        {
            g_rtc_backup_was_lost = 1U;
        }

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
        BKP_WriteBackupRegister(BKP_DR10, RTC_BACKUP_MARKER);
        *used_backup_time = 0U;
        return 1U;
    }

    /* Marker valid: backup domain was preserved, no battery failure */
    g_rtc_backup_was_lost = 0U;

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

/**
 * @brief  初始化 RTC 驱动
 * @param  default_time: 默认时间结构指针
 * @retval RTC 时间来源类型
 */
DrvRtcSource_t Drv_Rtc_Init(const DrvRtcTime_t *default_time)
{
    DrvRtcTime_t initial_time;   /* 本次 RTC 初始化采用的起始时间 */
    uint8_t used_backup_time;    /* 是否使用了后备域已有时间 */

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

    g_software_seconds = Drv_Rtc_TimeToSeconds(&initial_time);
    g_software_ms = 0U;
    g_software_date.year = 2026U;
    g_software_date.month = 5U;
    g_software_date.day = 7U;
    used_backup_time = 0U;

#if APP_CLOCK_SIM_ENABLED
    g_rtc_hardware_active = 0U;
    return DRV_RTC_SOURCE_SOFTWARE;
#else
    if (Drv_Rtc_InitHardware(g_software_seconds, &used_backup_time) != 0U)
    {
        g_rtc_hardware_active = 1U;
        return (used_backup_time != 0U) ? DRV_RTC_SOURCE_HARDWARE_BACKUP : DRV_RTC_SOURCE_HARDWARE_DEFAULT_TIME;
    }

    g_rtc_hardware_active = 0U;
    return DRV_RTC_SOURCE_SOFTWARE;
#endif
}

/**
 * @brief  RTC 1ms 周期任务
 * @param  无
 * @retval 无
 */
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
        g_software_seconds++;
        if (g_software_seconds >= RTC_SECONDS_PER_DAY)
        {
            g_software_seconds = 0UL;
            Drv_Rtc_AdvanceDate(&g_software_date);
        }
    }
}

/**
 * @brief  获取当前 RTC 时间
 * @param  time: 时间结构输出指针
 * @retval 无
 */
void Drv_Rtc_GetTime(DrvRtcTime_t *time)
{
    uint32_t seconds;  /* 当前 RTC 对应的当天秒数 */

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

/**
 * @brief  设置当前 RTC 时间
 * @param  time: 时间结构指针
 * @retval 无
 */
void Drv_Rtc_SetTime(const DrvRtcTime_t *time)
{
    uint32_t seconds;  /* 待写入 RTC 的当天秒数 */

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

/**
 * @brief  查询当前是否正在使用硬件 RTC
 * @param  无
 * @retval 1 表示使用硬件 RTC，0 表示使用软件时基
 */
uint8_t Drv_Rtc_IsHardwareActive(void)
{
    return g_rtc_hardware_active;
}

void Drv_Rtc_GetDate(DrvRtcDate_t *date)
{
    if (date == 0)
    {
        return;
    }

    if (g_rtc_hardware_active != 0U)
    {
        uint32_t total_days;
        DrvRtcDate_t temp;
        uint32_t i;

        /* Compute days from epoch 2026-01-01 using RTC counter as total seconds */
        total_days = RTC_GetCounter() / RTC_SECONDS_PER_DAY;
        temp.year = 2026U;
        temp.month = 1U;
        temp.day = 1U;

        for (i = 0UL; i < total_days; i++)
        {
            Drv_Rtc_AdvanceDate(&temp);
        }
        *date = temp;
        return;
    }

    *date = g_software_date;
}

void Drv_Rtc_SetDate(const DrvRtcDate_t *date)
{
    if ((date == 0) ||
        (date->year < 2000U) || (date->year > 2099U) ||
        (date->month < 1U) || (date->month > 12U) ||
        (date->day < 1U) || (date->day > 31U))
    {
        return;
    }

    g_software_date = *date;

    if (g_rtc_hardware_active != 0U)
    {
        DrvRtcDate_t epoch;
        uint32_t total_days;
        uint32_t seconds_within_day;

        epoch.year = 2026U;
        epoch.month = 1U;
        epoch.day = 1U;
        total_days = 0UL;

        while ((epoch.year < date->year) ||
               (epoch.year == date->year && epoch.month < date->month) ||
               (epoch.year == date->year && epoch.month == date->month && epoch.day < date->day))
        {
            Drv_Rtc_AdvanceDate(&epoch);
            total_days++;
        }

        seconds_within_day = RTC_GetCounter() % RTC_SECONDS_PER_DAY;
        RTC_WaitForLastTask();
        RTC_SetCounter(total_days * RTC_SECONDS_PER_DAY + seconds_within_day);
        RTC_WaitForLastTask();
    }
}

uint8_t Drv_Rtc_GetDayOfWeek(const DrvRtcDate_t *date)
{
    uint16_t y;
    uint8_t m;
    uint16_t k;
    uint16_t j;
    int32_t h;

    /* Zeller's Congruence: returns 0=Saturday, 1=Sunday, ..., 6=Friday */
    y = date->year;
    m = date->month;
    if (m < 3U)
    {
        m += 12U;
        y--;
    }
    k = y % 100U;
    j = y / 100U;
    h = ((int32_t)date->day
         + (int32_t)((13U * ((uint16_t)m + 1U)) / 5U)
         + (int32_t)k
         + (int32_t)(k / 4U)
         + (int32_t)(j / 4U)
         - (int32_t)(2U * j)) % 7;

    /* Normalize h to 0..6 (C99 % may produce negative), then convert:
       Zeller 0=Sat→6, 1=Sun→0, 2=Mon→1, ... i.e. desired = (h+6)%7 */
    h = ((h % 7) + 7) % 7;
    return (uint8_t)((h + 6U) % 7U);
}

uint8_t Drv_Rtc_WasBackupLost(void)
{
    return g_rtc_backup_was_lost;
}

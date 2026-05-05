#include "drv_rtc.h"

#include "sim_debug_config.h"
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
        g_software_seconds = (g_software_seconds + 1UL) % RTC_SECONDS_PER_DAY;
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

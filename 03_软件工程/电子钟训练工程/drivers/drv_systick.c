#include "drv_systick.h"


static volatile uint32_t g_systick_ms = 0U;  /* 全局 1ms 系统节拍计数 */

/**
 * @brief  初始化 SysTick 定时器来产生 1ms 的系统节拍
 * @param  无
 * @retval 无
 */
void Drv_Systick_Init(void)
{
#if APP_CLOCK_SIM_ENABLED
    g_systick_ms = 0U;
#else
    SysTick_Config(SystemCoreClock / 1000U);
#endif
}

/**
 * @brief  SysTick 中断处理函数，负责累加系统节拍
 * @param  无
 * @retval 无
 */
void Drv_Systick_IrqHandler(void)
{
    g_systick_ms++;
}

/**
 * @brief  返回当前系统时间戳
 * @param  无
 * @retval 当前系统毫秒计数
 */
uint32_t Drv_Systick_Millis(void)
{
    return g_systick_ms;
}

/**
 * @brief  判断距离上次事件是否已经过了指定的时间周期
 * @param  last_tick: 上次事件时间戳指针
 * @param  period_ms: 需要判断的周期毫秒值
 * @retval 1 表示已到周期，0 表示未到周期
 */
uint8_t Drv_Systick_IsElapsed(uint32_t *last_tick, uint32_t period_ms)
{
    uint32_t now;  /* 当前系统毫秒计数 */

    now = Drv_Systick_Millis();
    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1U;
    }
    return 0U;
}

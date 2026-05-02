#include "drv_systick.h"

static volatile uint32_t g_systick_ms = 0U;

/**
 * @brief  初始化 SysTick 定时器来产生 1ms 的系统节拍
 */
void Drv_Systick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

/**
 * @brief  全局系统时间戳 g_systick_ms 更新函数
 */
void Drv_Systick_IrqHandler(void)
{
    g_systick_ms++;
}

/**
 * @brief  返回当前系统时间戳
 */
uint32_t Drv_Systick_Millis(void)
{
    return g_systick_ms;
}

/**
 * @brief  判断距离上次事件是否已经过了指定的时间周期
 */
uint8_t Drv_Systick_IsElapsed(uint32_t *last_tick, uint32_t period_ms)
{
    uint32_t now;

    now = Drv_Systick_Millis();
    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1U;
    }
    return 0U;
}

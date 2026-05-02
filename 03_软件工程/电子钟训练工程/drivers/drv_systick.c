/* 系统节拍驱动：
 * 1. 配置 SysTick 每 1ms 产生一次中断
 * 2. 在中断里维护全局毫秒计数
 * 3. 为上层提供非阻塞定时判断
 */
#include "drv_systick.h"

static volatile uint32_t g_systick_ms = 0U;

void Drv_Systick_Init(void)
{
    /* 把 SysTick 配成 1ms 一次。 */
    SysTick_Config(SystemCoreClock / 1000U);
}

void Drv_Systick_IrqHandler(void)
{
    /* 在 SysTick 中断时调用，进行计数更新。 */
    g_systick_ms++;
}

uint32_t Drv_Systick_Millis(void)
{
    return g_systick_ms;
}

uint8_t Drv_Systick_IsElapsed(uint32_t *last_tick, uint32_t period_ms)
{
    uint32_t now;

    now = Drv_Systick_Millis();
    /* 使用时间差判断是否到期，这种写法天然支持计数器回绕。 */
    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1U;
    }
    return 0U;
}

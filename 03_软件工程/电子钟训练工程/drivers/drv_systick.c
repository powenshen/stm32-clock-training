#include "drv_systick.h"

static volatile uint32_t g_systick_ms = 0U;

void Drv_Systick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

void Drv_Systick_IrqHandler(void)
{
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
    if ((now - *last_tick) >= period_ms)
    {
        *last_tick = now;
        return 1U;
    }
    return 0U;
}

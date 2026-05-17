#include "stm32f10x.h"
#include "app_clock.h"

/* 上电即点灯：PB5/PB0/PB1 全亮，证明 CPU 已进入 main() */
static void AliveLED_On(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef g;
    g.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);
    GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5);  /* 全亮 (active-low) */
}

int main(void)
{
    AliveLED_On();   /* ← 第一个操作：点灯。亮了说明 CPU 进了 main() */

    App_Clock_Init();

    while (1)
    {
        App_Clock_Task();
    }
}

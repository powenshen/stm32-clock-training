#include "stm32f10x.h"

void Delay(volatile uint32_t count)
{
    while(count--);
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 全部熄灭，野火指南者 LED 是低电平亮
    GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5);
}

int main(void)
{
    LED_Init();

    while(1)
    {
        // 红灯亮：PB5 = 0
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
        Delay(8000000);

        // 绿灯亮：PB0 = 0
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1 | GPIO_Pin_5);
        Delay(8000000);

        // 蓝灯亮：PB1 = 0
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_5);
        Delay(8000000);

        // 全灭
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5);
        Delay(8000000);
    }
}
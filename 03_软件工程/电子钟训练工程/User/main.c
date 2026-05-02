#include "stm32f10x.h"

#include "app_clock.h"

/**
 * @brief  Main program.
 * 1. 调用 App_Clock_Init() 初始化时钟应用
 * 2. 进入死循环，在循环中调用 App_Clock_Task() 执行时钟应用的主任务函数
 */
int main(void)
{
    App_Clock_Init();

    while (1)
    {
        App_Clock_Task();
    }
}

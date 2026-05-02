/* 主流程入口：
 * 1. 上电后先完成电子钟相关模块初始化
 * 2. 随后在 while(1) 中不断轮询业务任务
 * 3. 周期性的小任务由中断驱动，主循环不进行 delay，只负责调度业务层
 */
#include "stm32f10x.h"

#include "app_clock.h"

int main(void)
{
    App_Clock_Init();

    while (1)
    {
        App_Clock_Task();
    }
}

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

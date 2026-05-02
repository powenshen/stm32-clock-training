/* 电子钟业务层：
 * 1. 维护当前时间和模式
 * 2. 决定按键事件在当前模式下代表什么含义
 * 3. 组合驱动层提供的节拍、按键和串口能力
 */
#include "app_clock.h"

#include <stdio.h>

#include "bsp_led.h"
#include "drv_debug_uart.h"
#include "drv_key.h"
#include "drv_systick.h"

typedef enum
{
    APP_MODE_RUN = 0,
    APP_MODE_SET_MINUTE
} AppMode_t;

typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    AppMode_t mode;
} ClockState_t;

static ClockState_t g_clock = {12U, 0U, 0U, APP_MODE_RUN};

static void App_Clock_PrintState(const char *reason)
{
    /* 统一从这里打印当前时钟状态，便于串口观察执行过程。 */
    printf("[%s] %02u:%02u:%02u mode=%s\n",
           reason,
           g_clock.hour,
           g_clock.minute,
           g_clock.second,
           (g_clock.mode == APP_MODE_RUN) ? "RUN" : "SET_MINUTE");
}

static void App_Clock_TickOneSecond(void)
{
    /* 电子钟最基本的走时逻辑：秒满 60 进位到分钟，分钟满 60 进位到小时。 */
    g_clock.second++;
    if (g_clock.second >= 60U)
    {
        g_clock.second = 0U;
        g_clock.minute++;
        if (g_clock.minute >= 60U)
        {
            g_clock.minute = 0U;
            g_clock.hour = (uint8_t)((g_clock.hour + 1U) % 24U);
        }
    }
}

static void App_Clock_HandleKeyEvent(KeyEvent_t event)
{
    /* 按键事件由驱动层产生，业务层只负责解释这个事件该触发什么动作。 */
    if (event == KEY_EVENT_NONE)
    {
        return;
    }

    if (event == KEY_EVENT_LONG_PRESS)
    {
        g_clock.mode = (g_clock.mode == APP_MODE_RUN) ? APP_MODE_SET_MINUTE : APP_MODE_RUN;
        App_Clock_PrintState("mode-switch");
        return;
    }

    if (g_clock.mode == APP_MODE_SET_MINUTE)
    {
        g_clock.minute = (uint8_t)((g_clock.minute + 1U) % 60U);
        App_Clock_PrintState("minute++");
    }
    else
    {
        BSP_LED_Toggle();
        App_Clock_PrintState("click");
    }
}

void App_Clock_Init(void)
{
    /* 初始化顺序体现依赖关系：
     * 先准备板级资源和驱动，再启动系统节拍，最后输出调试信息。
     */
    BSP_LED_Init();
    Drv_Key_Init();
    Drv_DebugUart_Init();
    Drv_Systick_Init();

    Drv_DebugUart_SendString("Clock training project ready.\n");
    Drv_DebugUart_SendString("Short press: toggle LED in RUN mode.\n");
    Drv_DebugUart_SendString("Long press: switch RUN/SET_MINUTE.\n");
    Drv_DebugUart_SendString("Short press in SET_MINUTE: minute++.\n");
    App_Clock_PrintState("boot");
}

void App_Clock_Task1ms(void)
{
    /* 该函数在 1ms 中断节拍里被调用。 */
    Drv_Key_Task1ms();
}

void App_Clock_Task(void)
{
    /* 记录上一次执行某个周期动作的时间戳，配合毫秒节拍实现非阻塞定时。 */
    static uint32_t last_led_tick = 0U;
    static uint32_t last_second_tick = 0U;

    App_Clock_HandleKeyEvent(Drv_Key_GetEvent());

    /* 运行模式下每 500ms 翻转一次 LED，用于观察系统是否在正常运行。 */
    if ((g_clock.mode == APP_MODE_RUN) && Drv_Systick_IsElapsed(&last_led_tick, 500U))
    {
        BSP_LED_Toggle();
    }

    /* 每 1000ms 触发一次“秒级任务”。 */
    if (Drv_Systick_IsElapsed(&last_second_tick, 1000U))
    {
        if (g_clock.mode == APP_MODE_RUN)
        {
            App_Clock_TickOneSecond();
        }
        App_Clock_PrintState("tick");
    }
}

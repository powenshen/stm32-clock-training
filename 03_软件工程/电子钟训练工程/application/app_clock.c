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
    printf("[%s] %02u:%02u:%02u mode=%s\n",
           reason,
           g_clock.hour,
           g_clock.minute,
           g_clock.second,
           (g_clock.mode == APP_MODE_RUN) ? "RUN" : "SET_MINUTE");
}

static void App_Clock_TickOneSecond(void)
{
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
    Drv_Key_Task1ms();
}

void App_Clock_Task(void)
{
    static uint32_t last_led_tick = 0U;
    static uint32_t last_second_tick = 0U;

    App_Clock_HandleKeyEvent(Drv_Key_GetEvent());

    if ((g_clock.mode == APP_MODE_RUN) && Drv_Systick_IsElapsed(&last_led_tick, 500U))
    {
        BSP_LED_Toggle();
    }

    if (Drv_Systick_IsElapsed(&last_second_tick, 1000U))
    {
        if (g_clock.mode == APP_MODE_RUN)
        {
            App_Clock_TickOneSecond();
        }
        App_Clock_PrintState("tick");
    }
}

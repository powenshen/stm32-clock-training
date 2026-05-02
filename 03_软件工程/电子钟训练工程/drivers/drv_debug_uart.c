/* 串口调试驱动：
 * 1. 初始化调试串口 GPIO 和 USART
 * 2. 提供字符和字符串发送能力
 * 3. 把 printf 重定向到串口，方便观察运行日志
 */
#include "drv_debug_uart.h"

#include <stdio.h>

#include "board_config.h"

void Drv_DebugUart_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    USART_InitTypeDef usart_init_structure;

    /* 调试串口的 GPIO 和 USART 外设都挂在 APB2，上电后先开时钟。 */
    RCC_APB2PeriphClockCmd(BOARD_DEBUG_GPIO_RCC | BOARD_DEBUG_UART_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_DEBUG_TX_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_DEBUG_TX_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BOARD_DEBUG_RX_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BOARD_DEBUG_RX_PORT, &gpio_init_structure);

    usart_init_structure.USART_BaudRate = BOARD_DEBUG_BAUDRATE;
    usart_init_structure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init_structure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart_init_structure.USART_Parity = USART_Parity_No;
    usart_init_structure.USART_StopBits = USART_StopBits_1;
    usart_init_structure.USART_WordLength = USART_WordLength_8b;
    USART_Init(BOARD_DEBUG_UART, &usart_init_structure);

    USART_Cmd(BOARD_DEBUG_UART, ENABLE);
}

void Drv_DebugUart_SendChar(char ch)
{
    USART_SendData(BOARD_DEBUG_UART, (uint16_t)ch);
    /* 等待发送数据寄存器空，确保下一个字符不会覆盖当前发送。 */
    while (USART_GetFlagStatus(BOARD_DEBUG_UART, USART_FLAG_TXE) == RESET)
    {
    }
}

void Drv_DebugUart_SendString(const char *text)
{
    /* 兼容常见串口终端，遇到 '\n' 时补发一个 '\r'。 */
    while (*text != '\0')
    {
        if (*text == '\n')
        {
            Drv_DebugUart_SendChar('\r');
        }
        Drv_DebugUart_SendChar(*text);
        text++;
    }
}

struct __FILE
{
    int handle;
};

FILE __stdout;

int fputc(int ch, FILE *stream)
{
    /* 让标准库 printf 最终走到调试串口。 */
    (void)stream;
    Drv_DebugUart_SendChar((char)ch);
    return ch;
}

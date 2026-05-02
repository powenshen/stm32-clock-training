#include "drv_debug_uart.h"

#include <stdio.h>

#include "board_config.h"

void Drv_DebugUart_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    USART_InitTypeDef usart_init_structure;

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
    while (USART_GetFlagStatus(BOARD_DEBUG_UART, USART_FLAG_TXE) == RESET)
    {
    }
}

void Drv_DebugUart_SendString(const char *text)
{
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
    (void)stream;
    Drv_DebugUart_SendChar((char)ch);
    return ch;
}

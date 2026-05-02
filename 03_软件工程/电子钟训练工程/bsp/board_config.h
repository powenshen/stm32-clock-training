#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f10x.h"

#define BOARD_LED_RCC              RCC_APB2Periph_GPIOC
#define BOARD_LED_PORT             GPIOC
#define BOARD_LED_PIN              GPIO_Pin_13
#define BOARD_LED_ACTIVE_LOW       1U

#define BOARD_KEY_RCC              RCC_APB2Periph_GPIOA
#define BOARD_KEY_PORT             GPIOA
#define BOARD_KEY_PIN              GPIO_Pin_0
#define BOARD_KEY_ACTIVE_LOW       1U

#define BOARD_DEBUG_UART_RCC       RCC_APB2Periph_USART1
#define BOARD_DEBUG_GPIO_RCC       RCC_APB2Periph_GPIOA
#define BOARD_DEBUG_UART           USART1
#define BOARD_DEBUG_TX_PORT        GPIOA
#define BOARD_DEBUG_TX_PIN         GPIO_Pin_9
#define BOARD_DEBUG_RX_PORT        GPIOA
#define BOARD_DEBUG_RX_PIN         GPIO_Pin_10
#define BOARD_DEBUG_BAUDRATE       115200U

#endif

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f10x.h"

/* 板级引脚配置表：
 * 1. 集中定义 LED、按键、调试串口连接到哪个端口和引脚
 * 2. 当硬件原理图确认后，优先在这里修改映射，而不是到处改业务代码
 * 3. ACTIVE_LOW 表示该设备以低电平作为“有效状态”
 */

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

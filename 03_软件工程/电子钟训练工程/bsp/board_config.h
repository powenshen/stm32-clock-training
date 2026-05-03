/*
 * Board resource map for the STM32 electronic clock training project.
 * Keep all MCU pin bindings here so higher layers do not depend on raw pins.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f10x.h"

#define BOARD_LED_RCC              RCC_APB2Periph_GPIOB
#define BOARD_LED_RED_PORT         GPIOB
#define BOARD_LED_RED_PIN          GPIO_Pin_5
#define BOARD_LED_GREEN_PORT       GPIOB
#define BOARD_LED_GREEN_PIN        GPIO_Pin_0
#define BOARD_LED_BLUE_PORT        GPIOB
#define BOARD_LED_BLUE_PIN         GPIO_Pin_1
#define BOARD_LED_ACTIVE_LOW       1U

#define BOARD_KEY1_RCC             RCC_APB2Periph_GPIOA
#define BOARD_KEY1_PORT            GPIOA
#define BOARD_KEY1_PIN             GPIO_Pin_0

#define BOARD_KEY2_RCC             RCC_APB2Periph_GPIOC
#define BOARD_KEY2_PORT            GPIOC
#define BOARD_KEY2_PIN             GPIO_Pin_13

#define BOARD_KEY_ACTIVE_LOW       1U
#define BOARD_KEY_COUNT            2U

#define BOARD_BUZZER_RCC           RCC_APB2Periph_GPIOA
#define BOARD_BUZZER_PORT          GPIOA
#define BOARD_BUZZER_PIN           GPIO_Pin_8
/* PA8 polarity is still subject to on-board verification. */
#define BOARD_BUZZER_ACTIVE_LOW    0U

#define BOARD_IR_GPIO_RCC          RCC_APB2Periph_GPIOE
#define BOARD_IR_AFIO_RCC          RCC_APB2Periph_AFIO
#define BOARD_IR_PORT              GPIOE
#define BOARD_IR_PIN               GPIO_Pin_5
#define BOARD_IR_PIN_SOURCE        GPIO_PinSource5
#define BOARD_IR_PORT_SOURCE       GPIO_PortSourceGPIOE

#define BOARD_DEBUG_UART_RCC       RCC_APB2Periph_USART1
#define BOARD_DEBUG_GPIO_RCC       RCC_APB2Periph_GPIOA
#define BOARD_DEBUG_UART           USART1
#define BOARD_DEBUG_TX_PORT        GPIOA
#define BOARD_DEBUG_TX_PIN         GPIO_Pin_9
#define BOARD_DEBUG_RX_PORT        GPIOA
#define BOARD_DEBUG_RX_PIN         GPIO_Pin_10
#define BOARD_DEBUG_BAUDRATE       115200U

#endif

/* LED 板级封装：
 * 1. 把具体端口和引脚包装成“初始化/开/关/翻转”接口
 * 2. 对上层隐藏高低电平有效的差异
 */
#include "bsp_led.h"

#include "board_config.h"

static void BSP_LED_Write(BitAction state)
{
    /* active-low 板子需要把逻辑亮灭转换成实际输出电平。 */
#if BOARD_LED_ACTIVE_LOW
    GPIO_WriteBit(BOARD_LED_PORT, BOARD_LED_PIN, (BitAction)!state);
#else
    GPIO_WriteBit(BOARD_LED_PORT, BOARD_LED_PIN, state);
#endif
}

void BSP_LED_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    /* 板级初始化只关心“这个 LED 接在哪个脚、这个脚怎么配”。 */
    RCC_APB2PeriphClockCmd(BOARD_LED_RCC, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_LED_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_LED_PORT, &gpio_init_structure);

    BSP_LED_Off();
}

void BSP_LED_On(void)
{
    BSP_LED_Write(Bit_SET);
}

void BSP_LED_Off(void)
{
    BSP_LED_Write(Bit_RESET);
}

void BSP_LED_Toggle(void)
{
    BitAction next_state;

    /* 先读当前输出状态，再计算翻转后的逻辑状态。 */
    next_state = (BitAction)(1U - GPIO_ReadOutputDataBit(BOARD_LED_PORT, BOARD_LED_PIN));
#if BOARD_LED_ACTIVE_LOW
    next_state = (BitAction)!next_state;
#endif
    BSP_LED_Write(next_state);
}

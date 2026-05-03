/*
 * Infrared receiver implementation for a PE5-connected 1838/HS0038 style module.
 * The decoder assumes a common NEC protocol and uses falling-edge intervals for robustness.
 */
#include "drv_ir_remote.h"

#include "board_config.h"
#include "misc.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

#define IR_FRAME_GAP_MIN_US        12000UL
#define IR_FRAME_GAP_MAX_US        14500UL
#define IR_REPEAT_GAP_MIN_US       10000UL
#define IR_REPEAT_GAP_MAX_US       12000UL
#define IR_BIT0_GAP_MIN_US         900UL
#define IR_BIT0_GAP_MAX_US         1400UL
#define IR_BIT1_GAP_MIN_US         1900UL
#define IR_BIT1_GAP_MAX_US         2600UL
#define IR_IDLE_RESET_GAP_US       30000UL

typedef struct
{
    uint8_t edge_seen;         /* 是否已经捕获到第一条下降沿 */
    uint8_t receiving;         /* 当前是否处于接收一帧数据状态 */
    uint8_t bit_index;         /* 当前已接收的位索引 */
    uint8_t has_last_frame;    /* 是否保存了上一条合法命令帧 */
    uint32_t last_edge_tick;   /* 上一次下降沿时间戳 */
    uint32_t frame_data;       /* 正在组装的 32 位 NEC 原始帧 */
    uint16_t last_address;     /* 上一条合法帧的地址码 */
    uint8_t last_command;      /* 上一条合法帧的命令码 */
} DrvIrRuntime_t;

static volatile DrvIrRemoteEvent_t g_ir_event = {0U, 0U, DRV_IR_REMOTE_EVENT_NONE};  /* 待主循环读取的红外事件缓存 */
static DrvIrRuntime_t g_ir_runtime = {0U, 0U, 0U, 0U, 0UL, 0UL, 0U, 0U};             /* 红外协议解析运行时状态 */

/**
 * @brief  向事件缓存压入一条红外事件
 * @param  address: 红外地址码
 * @param  command: 红外命令码
 * @param  type: 事件类型
 * @retval 无
 */
static void Drv_IrRemote_PushEvent(uint16_t address, uint8_t command, DrvIrRemoteEventType_t type)
{
    if (g_ir_event.type != DRV_IR_REMOTE_EVENT_NONE)
    {
        return;
    }

    g_ir_event.address = address;
    g_ir_event.command = command;
    g_ir_event.type = type;
}

/**
 * @brief  复位当前红外帧解析状态
 * @param  无
 * @retval 无
 */
static void Drv_IrRemote_ResetFrame(void)
{
    g_ir_runtime.receiving = 0U;
    g_ir_runtime.bit_index = 0U;
    g_ir_runtime.frame_data = 0UL;
}

/**
 * @brief  判断数值是否落在指定区间内
 * @param  value: 待判断值
 * @param  min_value: 区间最小值
 * @param  max_value: 区间最大值
 * @retval 1 表示在区间内，0 表示不在区间内
 */
static uint8_t Drv_IrRemote_IsInRange(uint32_t value, uint32_t min_value, uint32_t max_value)
{
    return (uint8_t)((value >= min_value) && (value <= max_value));
}

/**
 * @brief  对解析完成的 NEC 原始帧做合法性检查并提取命令
 * @param  frame_data: 32 位原始帧数据
 * @param  address: 输出地址码指针
 * @param  command: 输出命令码指针
 * @retval 1 表示帧合法，0 表示帧非法
 */
static uint8_t Drv_IrRemote_FinalizeFrame(uint32_t frame_data, uint16_t *address, uint8_t *command)
{
    uint8_t address_low;      /* 地址码低字节 */
    uint8_t address_high;     /* 地址码高字节或反码 */
    uint8_t command_value;    /* 命令码 */
    uint8_t command_inverse;  /* 命令码反码 */

    address_low = (uint8_t)(frame_data & 0xFFU);
    address_high = (uint8_t)((frame_data >> 8U) & 0xFFU);
    command_value = (uint8_t)((frame_data >> 16U) & 0xFFU);
    command_inverse = (uint8_t)((frame_data >> 24U) & 0xFFU);

    if ((uint8_t)(command_value ^ command_inverse) != 0xFFU)
    {
        return 0U;
    }

    if ((uint8_t)(address_low ^ address_high) == 0xFFU)
    {
        *address = address_low;
    }
    else
    {
        *address = (uint16_t)(((uint16_t)address_high << 8U) | address_low);
    }

    *command = command_value;
    return 1U;
}

/**
 * @brief  初始化红外遥控接收驱动
 * @param  无
 * @retval 无
 */
void Drv_IrRemote_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    EXTI_InitTypeDef exti_init_structure;
    NVIC_InitTypeDef nvic_init_structure;
    TIM_TimeBaseInitTypeDef tim_init_structure;

    RCC_APB2PeriphClockCmd(BOARD_IR_GPIO_RCC | BOARD_IR_AFIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    gpio_init_structure.GPIO_Pin = BOARD_IR_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_IR_PORT, &gpio_init_structure);

    TIM_TimeBaseStructInit(&tim_init_structure);
    tim_init_structure.TIM_Prescaler = (uint16_t)((SystemCoreClock / 1000000UL) - 1UL);
    /* A 1 us tick with a 16-bit auto-reload still covers all NEC gaps we decode. */
    tim_init_structure.TIM_Period = 0xFFFFU;
    tim_init_structure.TIM_ClockDivision = TIM_CKD_DIV1;
    tim_init_structure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim_init_structure);
    TIM_Cmd(TIM2, ENABLE);

    GPIO_EXTILineConfig(BOARD_IR_PORT_SOURCE, BOARD_IR_PIN_SOURCE);

    EXTI_ClearITPendingBit(EXTI_Line5);
    exti_init_structure.EXTI_Line = EXTI_Line5;
    exti_init_structure.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init_structure.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init_structure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init_structure);

    nvic_init_structure.NVIC_IRQChannel = EXTI9_5_IRQn;
    nvic_init_structure.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic_init_structure.NVIC_IRQChannelSubPriority = 0U;
    nvic_init_structure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init_structure);

    g_ir_event.type = DRV_IR_REMOTE_EVENT_NONE;
    g_ir_runtime.edge_seen = 0U;
    g_ir_runtime.receiving = 0U;
    g_ir_runtime.bit_index = 0U;
    g_ir_runtime.has_last_frame = 0U;
    g_ir_runtime.last_edge_tick = 0UL;
    g_ir_runtime.frame_data = 0UL;
    g_ir_runtime.last_address = 0U;
    g_ir_runtime.last_command = 0U;
}

/**
 * @brief  红外接收中断处理函数
 * @param  无
 * @retval 无
 */
void Drv_IrRemote_IrqHandler(void)
{
    uint32_t now_tick;    /* 当前下降沿对应的计时器计数值 */
    uint32_t delta_tick;  /* 与上一下降沿之间的时间间隔 */

    if (EXTI_GetITStatus(EXTI_Line5) == RESET)
    {
        return;
    }

    EXTI_ClearITPendingBit(EXTI_Line5);
    now_tick = TIM_GetCounter(TIM2);

    if (g_ir_runtime.edge_seen == 0U)
    {
        g_ir_runtime.edge_seen = 1U;
        g_ir_runtime.last_edge_tick = now_tick;
        return;
    }

    delta_tick = now_tick - g_ir_runtime.last_edge_tick;
    g_ir_runtime.last_edge_tick = now_tick;

    if (delta_tick >= IR_IDLE_RESET_GAP_US)
    {
        Drv_IrRemote_ResetFrame();
        return;
    }

    if (Drv_IrRemote_IsInRange(delta_tick, IR_FRAME_GAP_MIN_US, IR_FRAME_GAP_MAX_US) != 0U)
    {
        g_ir_runtime.receiving = 1U;
        g_ir_runtime.bit_index = 0U;
        g_ir_runtime.frame_data = 0UL;
        return;
    }

    if (Drv_IrRemote_IsInRange(delta_tick, IR_REPEAT_GAP_MIN_US, IR_REPEAT_GAP_MAX_US) != 0U)
    {
        if (g_ir_runtime.has_last_frame != 0U)
        {
            Drv_IrRemote_PushEvent(g_ir_runtime.last_address,
                                   g_ir_runtime.last_command,
                                   DRV_IR_REMOTE_EVENT_REPEAT);
        }
        Drv_IrRemote_ResetFrame();
        return;
    }

    if (g_ir_runtime.receiving == 0U)
    {
        return;
    }

    if (Drv_IrRemote_IsInRange(delta_tick, IR_BIT0_GAP_MIN_US, IR_BIT0_GAP_MAX_US) != 0U)
    {
        /* LSB-first NEC frame: a short interval represents bit 0. */
    }
    else if (Drv_IrRemote_IsInRange(delta_tick, IR_BIT1_GAP_MIN_US, IR_BIT1_GAP_MAX_US) != 0U)
    {
        g_ir_runtime.frame_data |= (1UL << g_ir_runtime.bit_index);
    }
    else
    {
        Drv_IrRemote_ResetFrame();
        return;
    }

    g_ir_runtime.bit_index++;
    if (g_ir_runtime.bit_index >= 32U)
    {
        uint16_t address;  /* 当前帧解析出的地址码 */
        uint8_t command;   /* 当前帧解析出的命令码 */

        if (Drv_IrRemote_FinalizeFrame(g_ir_runtime.frame_data, &address, &command) != 0U)
        {
            g_ir_runtime.last_address = address;
            g_ir_runtime.last_command = command;
            g_ir_runtime.has_last_frame = 1U;
            Drv_IrRemote_PushEvent(address, command, DRV_IR_REMOTE_EVENT_COMMAND);
        }

        Drv_IrRemote_ResetFrame();
    }
}

/**
 * @brief  读取一条红外事件并清空事件缓存
 * @param  无
 * @retval 读取到的红外事件
 */
DrvIrRemoteEvent_t Drv_IrRemote_GetEvent(void)
{
    DrvIrRemoteEvent_t event;  /* 返回给上层的红外事件 */

    __disable_irq();
    event = g_ir_event;
    g_ir_event.address = 0U;
    g_ir_event.command = 0U;
    g_ir_event.type = DRV_IR_REMOTE_EVENT_NONE;
    __enable_irq();

    return event;
}

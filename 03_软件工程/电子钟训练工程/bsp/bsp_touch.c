#include "bsp_touch.h"

#define BSP_TOUCH_PENIRQ_GPIO_CLK      RCC_APB2Periph_GPIOE
#define BSP_TOUCH_PENIRQ_GPIO_PORT     GPIOE
#define BSP_TOUCH_PENIRQ_GPIO_PIN      GPIO_Pin_4
#define BSP_TOUCH_PENIRQ_ACTIVE_LEVEL  0U

#define BSP_TOUCH_GPIO_CLK             (RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOD)
#define BSP_TOUCH_CS_PORT              GPIOD
#define BSP_TOUCH_CS_PIN               GPIO_Pin_13
#define BSP_TOUCH_CLK_PORT             GPIOE
#define BSP_TOUCH_CLK_PIN              GPIO_Pin_0
#define BSP_TOUCH_MOSI_PORT            GPIOE
#define BSP_TOUCH_MOSI_PIN             GPIO_Pin_2
#define BSP_TOUCH_MISO_PORT            GPIOE
#define BSP_TOUCH_MISO_PIN             GPIO_Pin_3

#define BSP_TOUCH_CHANNEL_X            0x90U
#define BSP_TOUCH_CHANNEL_Y            0xD0U

typedef struct
{
    float dx_x;
    float dx_y;
    float dx;
    float dy_x;
    float dy_y;
    float dy;
} BspTouchCalibration_t;

static const BspTouchCalibration_t g_touch_calibration[8] =
{
    {-0.006464f, -0.073259f, 280.358032f, 0.074878f, 0.002052f, -6.545977f},
    {0.086314f, 0.001891f, -12.836658f, -0.003722f, -0.065799f, 254.715714f},
    {0.002782f, 0.061522f, -11.595689f, 0.083393f, 0.005159f, -15.650089f},
    {0.089743f, -0.000289f, -20.612209f, -0.001374f, 0.064451f, -16.054003f},
    {0.000767f, -0.068258f, 250.891769f, -0.085559f, -0.000195f, 334.747650f},
    {-0.084744f, 0.000047f, 323.163147f, -0.002109f, -0.066371f, 260.985809f},
    {-0.001848f, 0.066984f, -12.807136f, -0.084858f, -0.000805f, 333.395386f},
    {-0.085470f, -0.000876f, 334.023163f, -0.003390f, 0.064725f, -6.211169f}
};

static void BSP_Touch_DelayUs(volatile uint32_t count)
{
    while (count > 0U)
    {
        volatile uint8_t inner;

        inner = 12U;
        while (inner > 0U)
        {
            inner--;
        }
        count--;
    }
}

static void BSP_Touch_CsDisable(void)
{
    GPIO_ResetBits(BSP_TOUCH_CS_PORT, BSP_TOUCH_CS_PIN);
}

static uint8_t BSP_Touch_IsPressed(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(BSP_TOUCH_PENIRQ_GPIO_PORT, BSP_TOUCH_PENIRQ_GPIO_PIN) == BSP_TOUCH_PENIRQ_ACTIVE_LEVEL);
}

static void BSP_Touch_ClkHigh(void)
{
    GPIO_SetBits(BSP_TOUCH_CLK_PORT, BSP_TOUCH_CLK_PIN);
}

static void BSP_Touch_ClkLow(void)
{
    GPIO_ResetBits(BSP_TOUCH_CLK_PORT, BSP_TOUCH_CLK_PIN);
}

static void BSP_Touch_MosiWrite(uint8_t high)
{
    if (high != 0U)
    {
        GPIO_SetBits(BSP_TOUCH_MOSI_PORT, BSP_TOUCH_MOSI_PIN);
    }
    else
    {
        GPIO_ResetBits(BSP_TOUCH_MOSI_PORT, BSP_TOUCH_MOSI_PIN);
    }
}

static uint8_t BSP_Touch_MisoRead(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(BSP_TOUCH_MISO_PORT, BSP_TOUCH_MISO_PIN);
}

static void BSP_Touch_WriteCmd(uint8_t cmd)
{
    uint8_t bit_index;

    BSP_Touch_MosiWrite(0U);
    BSP_Touch_ClkLow();

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        BSP_Touch_MosiWrite((uint8_t)((cmd & (uint8_t)(1U << (7U - bit_index))) != 0U));
        BSP_Touch_DelayUs(5U);
        BSP_Touch_ClkHigh();
        BSP_Touch_DelayUs(5U);
        BSP_Touch_ClkLow();
    }
}

static uint16_t BSP_Touch_ReadCmd(void)
{
    uint8_t bit_index;
    uint16_t value;

    value = 0U;
    BSP_Touch_MosiWrite(0U);
    BSP_Touch_ClkHigh();

    for (bit_index = 0U; bit_index < 12U; bit_index++)
    {
        BSP_Touch_ClkLow();
        value |= (uint16_t)(BSP_Touch_MisoRead() << (11U - bit_index));
        BSP_Touch_ClkHigh();
    }

    return value;
}

static uint16_t BSP_Touch_ReadAdc(uint8_t channel)
{
    BSP_Touch_WriteCmd(channel);
    return BSP_Touch_ReadCmd();
}

static void BSP_Touch_ReadAdcXY(int16_t *x_ad, int16_t *y_ad)
{
    *x_ad = (int16_t)BSP_Touch_ReadAdc(BSP_TOUCH_CHANNEL_X);
    BSP_Touch_DelayUs(1U);
    *y_ad = (int16_t)BSP_Touch_ReadAdc(BSP_TOUCH_CHANNEL_Y);
}

static uint8_t BSP_Touch_ReadSmoothRaw(int16_t *raw_x, int16_t *raw_y)
{
    uint8_t sample_count;
    uint8_t sample_index;
    int16_t sample_x[10];
    int16_t sample_y[10];
    int32_t x_min;
    int32_t x_max;
    int32_t y_min;
    int32_t y_max;
    int32_t x_sum;
    int32_t y_sum;

    sample_count = 0U;
    do
    {
        BSP_Touch_ReadAdcXY(&sample_x[sample_count], &sample_y[sample_count]);
        sample_count++;
    } while ((BSP_Touch_IsPressed() != 0U) && (sample_count < 10U));

    if ((sample_count < 10U) || (BSP_Touch_IsPressed() == 0U))
    {
        return 0U;
    }

    x_min = sample_x[0];
    x_max = sample_x[0];
    y_min = sample_y[0];
    y_max = sample_y[0];
    x_sum = 0L;
    y_sum = 0L;

    for (sample_index = 0U; sample_index < 10U; sample_index++)
    {
        if (sample_x[sample_index] < x_min)
        {
            x_min = sample_x[sample_index];
        }
        if (sample_x[sample_index] > x_max)
        {
            x_max = sample_x[sample_index];
        }
        if (sample_y[sample_index] < y_min)
        {
            y_min = sample_y[sample_index];
        }
        if (sample_y[sample_index] > y_max)
        {
            y_max = sample_y[sample_index];
        }

        x_sum += sample_x[sample_index];
        y_sum += sample_y[sample_index];
    }

    x_sum -= (x_min + x_max);
    y_sum -= (y_min + y_max);

    *raw_x = (int16_t)(x_sum >> 3);
    *raw_y = (int16_t)(y_sum >> 3);
    return 1U;
}

void BSP_Touch_Init(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    RCC_APB2PeriphClockCmd(BSP_TOUCH_GPIO_CLK | BSP_TOUCH_PENIRQ_GPIO_CLK, ENABLE);

    gpio_init_structure.GPIO_Pin = BSP_TOUCH_CLK_PIN;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_10MHz;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BSP_TOUCH_CLK_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BSP_TOUCH_MOSI_PIN;
    GPIO_Init(BSP_TOUCH_MOSI_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BSP_TOUCH_MISO_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BSP_TOUCH_MISO_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BSP_TOUCH_CS_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BSP_TOUCH_CS_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BSP_TOUCH_PENIRQ_GPIO_PIN;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BSP_TOUCH_PENIRQ_GPIO_PORT, &gpio_init_structure);

    BSP_Touch_CsDisable();
    BSP_Touch_ClkHigh();
}

uint8_t BSP_Touch_GetPoint(BspTouchPoint_t *point,
                           uint8_t scan_mode,
                           uint16_t lcd_width,
                           uint16_t lcd_height)
{
    int16_t raw_x;
    int16_t raw_y;
    float x;
    float y;

    if ((point == 0) || (scan_mode > 7U) || (BSP_Touch_IsPressed() == 0U))
    {
        return 0U;
    }

    if (BSP_Touch_ReadSmoothRaw(&raw_x, &raw_y) == 0U)
    {
        return 0U;
    }

    x = g_touch_calibration[scan_mode].dx_x * raw_x +
        g_touch_calibration[scan_mode].dx_y * raw_y +
        g_touch_calibration[scan_mode].dx;
    y = g_touch_calibration[scan_mode].dy_x * raw_x +
        g_touch_calibration[scan_mode].dy_y * raw_y +
        g_touch_calibration[scan_mode].dy;

    if ((x < 0.0f) || (y < 0.0f) || (x >= lcd_width) || (y >= lcd_height))
    {
        return 0U;
    }

    point->x = (uint16_t)x;
    point->y = (uint16_t)y;
    return 1U;
}

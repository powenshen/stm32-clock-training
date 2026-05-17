#include "bsp_lcd.h"

#include <string.h>

#include "sim_debug_config.h"

#define BSP_LCD_CMD_ADDR                ((uint32_t)0x60000000U)
#define BSP_LCD_DATA_ADDR               ((uint32_t)0x60020000U)
#define BSP_LCD_FSMC_BANK               FSMC_Bank1_NORSRAM1

#define BSP_LCD_CS_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_CS_PORT                 GPIOD
#define BSP_LCD_CS_PIN                  GPIO_Pin_7
#define BSP_LCD_DC_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_DC_PORT                 GPIOD
#define BSP_LCD_DC_PIN                  GPIO_Pin_11
#define BSP_LCD_WR_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_WR_PORT                 GPIOD
#define BSP_LCD_WR_PIN                  GPIO_Pin_5
#define BSP_LCD_RD_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_RD_PORT                 GPIOD
#define BSP_LCD_RD_PIN                  GPIO_Pin_4
#define BSP_LCD_RST_CLK                 RCC_APB2Periph_GPIOE
#define BSP_LCD_RST_PORT                GPIOE
#define BSP_LCD_RST_PIN                 GPIO_Pin_1
#define BSP_LCD_BK_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_BK_PORT                 GPIOD
#define BSP_LCD_BK_PIN                  GPIO_Pin_12

#define BSP_LCD_D0_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_D0_PORT                 GPIOD
#define BSP_LCD_D0_PIN                  GPIO_Pin_14
#define BSP_LCD_D1_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_D1_PORT                 GPIOD
#define BSP_LCD_D1_PIN                  GPIO_Pin_15
#define BSP_LCD_D2_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_D2_PORT                 GPIOD
#define BSP_LCD_D2_PIN                  GPIO_Pin_0
#define BSP_LCD_D3_CLK                  RCC_APB2Periph_GPIOD
#define BSP_LCD_D3_PORT                 GPIOD
#define BSP_LCD_D3_PIN                  GPIO_Pin_1
#define BSP_LCD_D4_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D4_PORT                 GPIOE
#define BSP_LCD_D4_PIN                  GPIO_Pin_7
#define BSP_LCD_D5_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D5_PORT                 GPIOE
#define BSP_LCD_D5_PIN                  GPIO_Pin_8
#define BSP_LCD_D6_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D6_PORT                 GPIOE
#define BSP_LCD_D6_PIN                  GPIO_Pin_9
#define BSP_LCD_D7_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D7_PORT                 GPIOE
#define BSP_LCD_D7_PIN                  GPIO_Pin_10
#define BSP_LCD_D8_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D8_PORT                 GPIOE
#define BSP_LCD_D8_PIN                  GPIO_Pin_11
#define BSP_LCD_D9_CLK                  RCC_APB2Periph_GPIOE
#define BSP_LCD_D9_PORT                 GPIOE
#define BSP_LCD_D9_PIN                  GPIO_Pin_12
#define BSP_LCD_D10_CLK                 RCC_APB2Periph_GPIOE
#define BSP_LCD_D10_PORT                GPIOE
#define BSP_LCD_D10_PIN                 GPIO_Pin_13
#define BSP_LCD_D11_CLK                 RCC_APB2Periph_GPIOE
#define BSP_LCD_D11_PORT                GPIOE
#define BSP_LCD_D11_PIN                 GPIO_Pin_14
#define BSP_LCD_D12_CLK                 RCC_APB2Periph_GPIOE
#define BSP_LCD_D12_PORT                GPIOE
#define BSP_LCD_D12_PIN                 GPIO_Pin_15
#define BSP_LCD_D13_CLK                 RCC_APB2Periph_GPIOD
#define BSP_LCD_D13_PORT                GPIOD
#define BSP_LCD_D13_PIN                 GPIO_Pin_8
#define BSP_LCD_D14_CLK                 RCC_APB2Periph_GPIOD
#define BSP_LCD_D14_PORT                GPIOD
#define BSP_LCD_D14_PIN                 GPIO_Pin_9
#define BSP_LCD_D15_CLK                 RCC_APB2Periph_GPIOD
#define BSP_LCD_D15_PORT                GPIOD
#define BSP_LCD_D15_PIN                 GPIO_Pin_10

#define BSP_LCD_CMD_SET_X               0x2AU
#define BSP_LCD_CMD_SET_Y               0x2BU
#define BSP_LCD_CMD_WRITE_GRAM          0x2CU

#define BSP_LCD_ID_UNKNOWN              0x0000U
#define BSP_LCD_ID_ILI9341              0x9341U
#define BSP_LCD_ID_ST7789V              0x8552U

#define BSP_LCD_FONT_WIDTH              5U
#define BSP_LCD_FONT_HEIGHT             7U

typedef struct
{
    char code;
    uint8_t rows[BSP_LCD_FONT_HEIGHT];
} BspLcdGlyph_t;

static uint16_t g_lcd_id = BSP_LCD_ID_UNKNOWN;
static uint16_t g_lcd_width = 320U;
static uint16_t g_lcd_height = 240U;
static uint8_t g_lcd_scan_mode = 3U;

static const BspLcdGlyph_t g_font[] =
{
    {' ', {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},
    {'+', {0x00U, 0x04U, 0x04U, 0x1FU, 0x04U, 0x04U, 0x00U}},
    {'-', {0x00U, 0x00U, 0x00U, 0x1FU, 0x00U, 0x00U, 0x00U}},
    {':', {0x00U, 0x04U, 0x04U, 0x00U, 0x04U, 0x04U, 0x00U}},
    {'(', {0x02U, 0x04U, 0x08U, 0x08U, 0x08U, 0x04U, 0x02U}},
    {')', {0x08U, 0x04U, 0x02U, 0x02U, 0x02U, 0x04U, 0x08U}},
    {'0', {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU}},
    {'1', {0x04U, 0x0CU, 0x14U, 0x04U, 0x04U, 0x04U, 0x1FU}},
    {'2', {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU}},
    {'3', {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU}},
    {'4', {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U}},
    {'5', {0x1FU, 0x10U, 0x1EU, 0x01U, 0x01U, 0x11U, 0x0EU}},
    {'6', {0x06U, 0x08U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU}},
    {'7', {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U}},
    {'8', {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU}},
    {'9', {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x02U, 0x1CU}},
    {'A', {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}},
    {'B', {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU}},
    {'C', {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU}},
    {'D', {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU}},
    {'E', {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU}},
    {'F', {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U}},
    {'G', {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0EU}},
    {'H', {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}},
    {'I', {0x0EU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU}},
    {'K', {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U}},
    {'L', {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU}},
    {'M', {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U}},
    {'N', {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U}},
    {'O', {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}},
    {'P', {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U}},
    {'R', {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U}},
    {'S', {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU}},
    {'T', {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U}},
    {'U', {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}},
    {'W', {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU}},
    {'Y', {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U}}
};

static void BSP_LCD_Delay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
    }
}

static void BSP_LCD_WriteCmd(uint16_t cmd)
{
    *(__IO uint16_t *)BSP_LCD_CMD_ADDR = cmd;
}

static void BSP_LCD_WriteData(uint16_t data)
{
    *(__IO uint16_t *)BSP_LCD_DATA_ADDR = data;
}

static uint16_t BSP_LCD_ReadData(void)
{
    return *(__IO uint16_t *)BSP_LCD_DATA_ADDR;
}

static void BSP_LCD_OpenWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_X);
    BSP_LCD_WriteData((uint16_t)(x >> 8));
    BSP_LCD_WriteData((uint16_t)(x & 0xFFU));
    BSP_LCD_WriteData((uint16_t)((x + width - 1U) >> 8));
    BSP_LCD_WriteData((uint16_t)((x + width - 1U) & 0xFFU));

    BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_Y);
    BSP_LCD_WriteData((uint16_t)(y >> 8));
    BSP_LCD_WriteData((uint16_t)(y & 0xFFU));
    BSP_LCD_WriteData((uint16_t)((y + height - 1U) >> 8));
    BSP_LCD_WriteData((uint16_t)((y + height - 1U) & 0xFFU));
}

static void BSP_LCD_FillColor(uint32_t amount, uint16_t color)
{
    uint32_t index;

    BSP_LCD_WriteCmd(BSP_LCD_CMD_WRITE_GRAM);
    for (index = 0UL; index < amount; index++)
    {
        BSP_LCD_WriteData(color);
    }
}

static void BSP_LCD_GPIO_Config(void)
{
    GPIO_InitTypeDef gpio_init_structure;

    RCC_APB2PeriphClockCmd(BSP_LCD_CS_CLK | BSP_LCD_DC_CLK | BSP_LCD_WR_CLK |
                           BSP_LCD_RD_CLK | BSP_LCD_BK_CLK | BSP_LCD_RST_CLK |
                           BSP_LCD_D0_CLK | BSP_LCD_D1_CLK | BSP_LCD_D2_CLK |
                           BSP_LCD_D3_CLK | BSP_LCD_D4_CLK | BSP_LCD_D5_CLK |
                           BSP_LCD_D6_CLK | BSP_LCD_D7_CLK | BSP_LCD_D8_CLK |
                           BSP_LCD_D9_CLK | BSP_LCD_D10_CLK | BSP_LCD_D11_CLK |
                           BSP_LCD_D12_CLK | BSP_LCD_D13_CLK | BSP_LCD_D14_CLK |
                           BSP_LCD_D15_CLK,
                           ENABLE);

    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_structure.GPIO_Mode = GPIO_Mode_AF_PP;

    gpio_init_structure.GPIO_Pin = BSP_LCD_D0_PIN;
    GPIO_Init(BSP_LCD_D0_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D1_PIN;
    GPIO_Init(BSP_LCD_D1_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D2_PIN;
    GPIO_Init(BSP_LCD_D2_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D3_PIN;
    GPIO_Init(BSP_LCD_D3_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D4_PIN;
    GPIO_Init(BSP_LCD_D4_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D5_PIN;
    GPIO_Init(BSP_LCD_D5_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D6_PIN;
    GPIO_Init(BSP_LCD_D6_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D7_PIN;
    GPIO_Init(BSP_LCD_D7_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D8_PIN;
    GPIO_Init(BSP_LCD_D8_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D9_PIN;
    GPIO_Init(BSP_LCD_D9_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D10_PIN;
    GPIO_Init(BSP_LCD_D10_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D11_PIN;
    GPIO_Init(BSP_LCD_D11_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D12_PIN;
    GPIO_Init(BSP_LCD_D12_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D13_PIN;
    GPIO_Init(BSP_LCD_D13_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D14_PIN;
    GPIO_Init(BSP_LCD_D14_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_D15_PIN;
    GPIO_Init(BSP_LCD_D15_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Pin = BSP_LCD_RD_PIN;
    GPIO_Init(BSP_LCD_RD_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_WR_PIN;
    GPIO_Init(BSP_LCD_WR_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_CS_PIN;
    GPIO_Init(BSP_LCD_CS_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_DC_PIN;
    GPIO_Init(BSP_LCD_DC_PORT, &gpio_init_structure);

    gpio_init_structure.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init_structure.GPIO_Pin = BSP_LCD_RST_PIN;
    GPIO_Init(BSP_LCD_RST_PORT, &gpio_init_structure);
    gpio_init_structure.GPIO_Pin = BSP_LCD_BK_PIN;
    GPIO_Init(BSP_LCD_BK_PORT, &gpio_init_structure);
}

static void BSP_LCD_FSMC_Config(void)
{
    FSMC_NORSRAMInitTypeDef fsmc_init_structure;
    FSMC_NORSRAMTimingInitTypeDef read_write_timing;

    memset(&fsmc_init_structure, 0, sizeof(fsmc_init_structure));
    memset(&read_write_timing, 0, sizeof(read_write_timing));

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    read_write_timing.FSMC_AddressSetupTime = 0x01U;
    read_write_timing.FSMC_DataSetupTime = 0x04U;
    read_write_timing.FSMC_AccessMode = FSMC_AccessMode_B;
    read_write_timing.FSMC_AddressHoldTime = 0x00U;
    read_write_timing.FSMC_BusTurnAroundDuration = 0x00U;
    read_write_timing.FSMC_CLKDivision = 0x00U;
    read_write_timing.FSMC_DataLatency = 0x00U;

    fsmc_init_structure.FSMC_Bank = BSP_LCD_FSMC_BANK;
    fsmc_init_structure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    fsmc_init_structure.FSMC_MemoryType = FSMC_MemoryType_NOR;
    fsmc_init_structure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
    fsmc_init_structure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    fsmc_init_structure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    fsmc_init_structure.FSMC_WrapMode = FSMC_WrapMode_Disable;
    fsmc_init_structure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    fsmc_init_structure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    fsmc_init_structure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    fsmc_init_structure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
    fsmc_init_structure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    fsmc_init_structure.FSMC_ReadWriteTimingStruct = &read_write_timing;
    fsmc_init_structure.FSMC_WriteTimingStruct = &read_write_timing;

    FSMC_NORSRAMInit(&fsmc_init_structure);
    FSMC_NORSRAMCmd(BSP_LCD_FSMC_BANK, ENABLE);
}

void BSP_LCD_BacklightOn(void)
{
#if !APP_CLOCK_SIM_ENABLED
    GPIO_ResetBits(BSP_LCD_BK_PORT, BSP_LCD_BK_PIN);
#endif
}

void BSP_LCD_BacklightOff(void)
{
#if !APP_CLOCK_SIM_ENABLED
    GPIO_SetBits(BSP_LCD_BK_PORT, BSP_LCD_BK_PIN);
#endif
}

static void BSP_LCD_Reset(void)
{
    /* LCD reset requires ≥10ms low pulse + ≥50ms recovery before commands.
       0xFFFFFU ≈ 1M volatile-loop iterations ≈ 50ms at 72 MHz. */
    GPIO_ResetBits(BSP_LCD_RST_PORT, BSP_LCD_RST_PIN);
    BSP_LCD_Delay(0x0FFFFFU);
    GPIO_SetBits(BSP_LCD_RST_PORT, BSP_LCD_RST_PIN);
    BSP_LCD_Delay(0x0FFFFFU);
}

static uint16_t BSP_LCD_ReadIdInternal(void)
{
    uint16_t id;

    BSP_LCD_WriteCmd(0x04U);
    BSP_LCD_ReadData();
    BSP_LCD_ReadData();
    id = BSP_LCD_ReadData();
    id <<= 8;
    id |= BSP_LCD_ReadData();

    if (id == BSP_LCD_ID_ST7789V)
    {
        return id;
    }

    BSP_LCD_WriteCmd(0xD3U);
    BSP_LCD_ReadData();
    BSP_LCD_ReadData();
    id = BSP_LCD_ReadData();
    id <<= 8;
    id |= BSP_LCD_ReadData();

    if (id == BSP_LCD_ID_ILI9341)
    {
        return id;
    }

    return BSP_LCD_ID_UNKNOWN;
}

static void BSP_LCD_ConfigureOrientation(uint8_t scan_mode)
{
    if (scan_mode > 7U)
    {
        scan_mode = 3U;
    }

    g_lcd_scan_mode = scan_mode;
    if ((scan_mode & 0x01U) == 0U)
    {
        g_lcd_width = 240U;
        g_lcd_height = 320U;
    }
    else
    {
        g_lcd_width = 320U;
        g_lcd_height = 240U;
    }

    BSP_LCD_WriteCmd(0x36U);
    if (g_lcd_id == BSP_LCD_ID_ILI9341)
    {
        BSP_LCD_WriteData((uint16_t)(0x08U | (scan_mode << 5)));
    }
    else
    {
        BSP_LCD_WriteData((uint16_t)(scan_mode << 5));
    }

    BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_X);
    BSP_LCD_WriteData(0x00U);
    BSP_LCD_WriteData(0x00U);
    BSP_LCD_WriteData((uint16_t)(((g_lcd_width - 1U) >> 8) & 0xFFU));
    BSP_LCD_WriteData((uint16_t)((g_lcd_width - 1U) & 0xFFU));

    BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_Y);
    BSP_LCD_WriteData(0x00U);
    BSP_LCD_WriteData(0x00U);
    BSP_LCD_WriteData((uint16_t)(((g_lcd_height - 1U) >> 8) & 0xFFU));
    BSP_LCD_WriteData((uint16_t)((g_lcd_height - 1U) & 0xFFU));

    BSP_LCD_WriteCmd(BSP_LCD_CMD_WRITE_GRAM);
}

static void BSP_LCD_RegConfig(void)
{
    g_lcd_id = BSP_LCD_ReadIdInternal();

    if (g_lcd_id == BSP_LCD_ID_ILI9341)
    {
        BSP_LCD_WriteCmd(0xCFU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x81U);
        BSP_LCD_WriteData(0x30U);

        BSP_LCD_WriteCmd(0xEDU);
        BSP_LCD_WriteData(0x64U);
        BSP_LCD_WriteData(0x03U);
        BSP_LCD_WriteData(0x12U);
        BSP_LCD_WriteData(0x81U);

        BSP_LCD_WriteCmd(0xE8U);
        BSP_LCD_WriteData(0x85U);
        BSP_LCD_WriteData(0x10U);
        BSP_LCD_WriteData(0x78U);

        BSP_LCD_WriteCmd(0xCBU);
        BSP_LCD_WriteData(0x39U);
        BSP_LCD_WriteData(0x2CU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x34U);
        BSP_LCD_WriteData(0x02U);

        BSP_LCD_WriteCmd(0xF7U);
        BSP_LCD_WriteData(0x20U);

        BSP_LCD_WriteCmd(0xEAU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0xB1U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x1BU);

        BSP_LCD_WriteCmd(0xB6U);
        BSP_LCD_WriteData(0x0AU);
        BSP_LCD_WriteData(0xA2U);

        BSP_LCD_WriteCmd(0xC0U);
        BSP_LCD_WriteData(0x35U);

        BSP_LCD_WriteCmd(0xC1U);
        BSP_LCD_WriteData(0x11U);

        BSP_LCD_WriteCmd(0xC5U);
        BSP_LCD_WriteData(0x45U);
        BSP_LCD_WriteData(0x45U);

        BSP_LCD_WriteCmd(0xC7U);
        BSP_LCD_WriteData(0xA2U);

        BSP_LCD_WriteCmd(0xF2U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0x26U);
        BSP_LCD_WriteData(0x01U);

        BSP_LCD_WriteCmd(0xE0U);
        BSP_LCD_WriteData(0x0FU);
        BSP_LCD_WriteData(0x26U);
        BSP_LCD_WriteData(0x24U);
        BSP_LCD_WriteData(0x0BU);
        BSP_LCD_WriteData(0x0EU);
        BSP_LCD_WriteData(0x09U);
        BSP_LCD_WriteData(0x54U);
        BSP_LCD_WriteData(0xA8U);
        BSP_LCD_WriteData(0x46U);
        BSP_LCD_WriteData(0x0CU);
        BSP_LCD_WriteData(0x17U);
        BSP_LCD_WriteData(0x09U);
        BSP_LCD_WriteData(0x0FU);
        BSP_LCD_WriteData(0x07U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0xE1U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x19U);
        BSP_LCD_WriteData(0x1BU);
        BSP_LCD_WriteData(0x04U);
        BSP_LCD_WriteData(0x10U);
        BSP_LCD_WriteData(0x07U);
        BSP_LCD_WriteData(0x2AU);
        BSP_LCD_WriteData(0x47U);
        BSP_LCD_WriteData(0x39U);
        BSP_LCD_WriteData(0x03U);
        BSP_LCD_WriteData(0x06U);
        BSP_LCD_WriteData(0x06U);
        BSP_LCD_WriteData(0x30U);
        BSP_LCD_WriteData(0x38U);
        BSP_LCD_WriteData(0x0FU);

        BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_X);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0xEFU);

        BSP_LCD_WriteCmd(BSP_LCD_CMD_SET_Y);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x01U);
        BSP_LCD_WriteData(0x3FU);
    }
    else
    {
        BSP_LCD_WriteCmd(0xCFU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0xC1U);
        BSP_LCD_WriteData(0x30U);

        BSP_LCD_WriteCmd(0xEDU);
        BSP_LCD_WriteData(0x64U);
        BSP_LCD_WriteData(0x03U);
        BSP_LCD_WriteData(0x12U);
        BSP_LCD_WriteData(0x81U);

        BSP_LCD_WriteCmd(0xE8U);
        BSP_LCD_WriteData(0x85U);
        BSP_LCD_WriteData(0x10U);
        BSP_LCD_WriteData(0x78U);

        BSP_LCD_WriteCmd(0xCBU);
        BSP_LCD_WriteData(0x39U);
        BSP_LCD_WriteData(0x2CU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x34U);
        BSP_LCD_WriteData(0x02U);

        BSP_LCD_WriteCmd(0xF7U);
        BSP_LCD_WriteData(0x20U);

        BSP_LCD_WriteCmd(0xEAU);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0xC0U);
        BSP_LCD_WriteData(0x21U);
        BSP_LCD_WriteCmd(0xC1U);
        BSP_LCD_WriteData(0x11U);

        BSP_LCD_WriteCmd(0xC5U);
        BSP_LCD_WriteData(0x2DU);
        BSP_LCD_WriteData(0x33U);

        BSP_LCD_WriteCmd(0x36U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0x3AU);
        BSP_LCD_WriteData(0x55U);

        BSP_LCD_WriteCmd(0xB1U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x17U);

        BSP_LCD_WriteCmd(0xB6U);
        BSP_LCD_WriteData(0x0AU);
        BSP_LCD_WriteData(0xA2U);

        BSP_LCD_WriteCmd(0xF6U);
        BSP_LCD_WriteData(0x01U);
        BSP_LCD_WriteData(0x30U);

        BSP_LCD_WriteCmd(0xF2U);
        BSP_LCD_WriteData(0x00U);

        BSP_LCD_WriteCmd(0x26U);
        BSP_LCD_WriteData(0x01U);

        BSP_LCD_WriteCmd(0xE0U);
        BSP_LCD_WriteData(0xD0U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x02U);
        BSP_LCD_WriteData(0x07U);
        BSP_LCD_WriteData(0x0BU);
        BSP_LCD_WriteData(0x1AU);
        BSP_LCD_WriteData(0x31U);
        BSP_LCD_WriteData(0x54U);
        BSP_LCD_WriteData(0x40U);
        BSP_LCD_WriteData(0x29U);
        BSP_LCD_WriteData(0x12U);
        BSP_LCD_WriteData(0x12U);
        BSP_LCD_WriteData(0x12U);
        BSP_LCD_WriteData(0x17U);

        BSP_LCD_WriteCmd(0xE1U);
        BSP_LCD_WriteData(0xD0U);
        BSP_LCD_WriteData(0x00U);
        BSP_LCD_WriteData(0x02U);
        BSP_LCD_WriteData(0x07U);
        BSP_LCD_WriteData(0x05U);
        BSP_LCD_WriteData(0x25U);
        BSP_LCD_WriteData(0x2DU);
        BSP_LCD_WriteData(0x44U);
        BSP_LCD_WriteData(0x45U);
        BSP_LCD_WriteData(0x1CU);
        BSP_LCD_WriteData(0x18U);
        BSP_LCD_WriteData(0x16U);
        BSP_LCD_WriteData(0x1CU);
        BSP_LCD_WriteData(0x1DU);
    }

    BSP_LCD_WriteCmd(0x11U);
    BSP_LCD_Delay(0x2BFFCU);
    BSP_LCD_WriteCmd(0x29U);
    BSP_LCD_WriteCmd(0x2CU);

    BSP_LCD_ConfigureOrientation(3U);
}

static const uint8_t *BSP_LCD_FindGlyph(char ch)
{
    uint32_t index;

    for (index = 0UL; index < (sizeof(g_font) / sizeof(g_font[0])); index++)
    {
        if (g_font[index].code == ch)
        {
            return g_font[index].rows;
        }
    }

    return g_font[0].rows;
}

static void BSP_LCD_DrawChar(uint16_t x,
                             uint16_t y,
                             char ch,
                             uint16_t fg_color,
                             uint16_t bg_color,
                             uint8_t scale)
{
    uint8_t row;
    uint8_t col;
    const uint8_t *glyph;

    glyph = BSP_LCD_FindGlyph(ch);
    for (row = 0U; row < BSP_LCD_FONT_HEIGHT; row++)
    {
        for (col = 0U; col < BSP_LCD_FONT_WIDTH; col++)
        {
            uint16_t pixel_color;

            pixel_color = ((glyph[row] & (uint8_t)(1U << (BSP_LCD_FONT_WIDTH - 1U - col))) != 0U) ? fg_color : bg_color;
            BSP_LCD_FillRect((uint16_t)(x + col * scale),
                             (uint16_t)(y + row * scale),
                             scale,
                             scale,
                             pixel_color);
        }
    }

    BSP_LCD_FillRect((uint16_t)(x + BSP_LCD_FONT_WIDTH * scale),
                     y,
                     scale,
                     (uint16_t)(BSP_LCD_FONT_HEIGHT * scale),
                     bg_color);
}

void BSP_LCD_Init(void)
{
    /* Wait for LCD power supply to stabilize (~80ms at 72MHz).
       Without this, the LCD controller may miss the reset pulse. */
    BSP_LCD_Delay(0x1FFFFFUL);

    BSP_LCD_GPIO_Config();
    BSP_LCD_FSMC_Config();
    BSP_LCD_BacklightOn();
    BSP_LCD_Reset();
    BSP_LCD_RegConfig();
    BSP_LCD_FillScreen(BSP_LCD_COLOR_BLACK);
}

uint16_t BSP_LCD_GetId(void)
{
    return g_lcd_id;
}

uint16_t BSP_LCD_GetWidth(void)
{
    return g_lcd_width;
}

uint16_t BSP_LCD_GetHeight(void)
{
    return g_lcd_height;
}

uint8_t BSP_LCD_GetScanMode(void)
{
    return g_lcd_scan_mode;
}

void BSP_LCD_FillScreen(uint16_t color)
{
    BSP_LCD_FillRect(0U, 0U, g_lcd_width, g_lcd_height, color);
}

void BSP_LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if ((x >= g_lcd_width) || (y >= g_lcd_height) || (width == 0U) || (height == 0U))
    {
        return;
    }

    if ((uint32_t)x + width > g_lcd_width)
    {
        width = (uint16_t)(g_lcd_width - x);
    }

    if ((uint32_t)y + height > g_lcd_height)
    {
        height = (uint16_t)(g_lcd_height - y);
    }

    BSP_LCD_OpenWindow(x, y, width, height);
    BSP_LCD_FillColor((uint32_t)width * height, color);
}

void BSP_LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if ((width < 2U) || (height < 2U))
    {
        BSP_LCD_FillRect(x, y, width, height, color);
        return;
    }

    BSP_LCD_FillRect(x, y, width, 1U, color);
    BSP_LCD_FillRect(x, (uint16_t)(y + height - 1U), width, 1U, color);
    BSP_LCD_FillRect(x, y, 1U, height, color);
    BSP_LCD_FillRect((uint16_t)(x + width - 1U), y, 1U, height, color);
}

void BSP_LCD_DrawString(uint16_t x,
                        uint16_t y,
                        const char *text,
                        uint16_t fg_color,
                        uint16_t bg_color,
                        uint8_t scale)
{
    uint16_t cursor_x;
    uint16_t index;
    uint16_t length;

    if ((text == 0) || (scale == 0U))
    {
        return;
    }

    cursor_x = x;
    length = (uint16_t)strlen(text);

    for (index = 0U; index < length; index++)
    {
        BSP_LCD_DrawChar(cursor_x, y, text[index], fg_color, bg_color, scale);
        cursor_x = (uint16_t)(cursor_x + (BSP_LCD_FONT_WIDTH + 1U) * scale);
    }
}

void BSP_LCD_DrawStringCentered(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                const char *text,
                                uint16_t fg_color,
                                uint16_t bg_color,
                                uint8_t scale)
{
    uint16_t text_width;
    uint16_t start_x;

    if ((text == 0) || (scale == 0U))
    {
        return;
    }

    text_width = (uint16_t)(strlen(text) * (BSP_LCD_FONT_WIDTH + 1U) * scale);
    if ((text_width > 0U) && (text_width >= scale))
    {
        text_width = (uint16_t)(text_width - scale);
    }

    start_x = x;
    if (text_width < width)
    {
        start_x = (uint16_t)(x + (width - text_width) / 2U);
    }

    BSP_LCD_DrawString(start_x, y, text, fg_color, bg_color, scale);
}

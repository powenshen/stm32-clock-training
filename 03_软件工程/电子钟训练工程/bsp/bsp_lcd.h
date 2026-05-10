#ifndef BSP_LCD_H
#define BSP_LCD_H

#include "stm32f10x.h"

#define BSP_LCD_COLOR_BLACK       0x0000U
#define BSP_LCD_COLOR_WHITE       0xFFFFU
#define BSP_LCD_COLOR_RED         0xF800U
#define BSP_LCD_COLOR_GREEN       0x07E0U
#define BSP_LCD_COLOR_BLUE        0x001FU
#define BSP_LCD_COLOR_CYAN        0x07FFU
#define BSP_LCD_COLOR_YELLOW      0xFFE0U
#define BSP_LCD_COLOR_MAGENTA     0xF81FU
#define BSP_LCD_COLOR_ORANGE      0xFD20U
#define BSP_LCD_COLOR_GRAY        0x8410U
#define BSP_LCD_COLOR_DARK_GRAY   0x4208U
#define BSP_LCD_COLOR_NAVY        0x10A2U

void BSP_LCD_Init(void);
uint16_t BSP_LCD_GetId(void);
uint16_t BSP_LCD_GetWidth(void);
uint16_t BSP_LCD_GetHeight(void);
uint8_t BSP_LCD_GetScanMode(void);

void BSP_LCD_FillScreen(uint16_t color);
void BSP_LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void BSP_LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void BSP_LCD_DrawString(uint16_t x,
                        uint16_t y,
                        const char *text,
                        uint16_t fg_color,
                        uint16_t bg_color,
                        uint8_t scale);
void BSP_LCD_DrawStringCentered(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                const char *text,
                                uint16_t fg_color,
                                uint16_t bg_color,
                                uint8_t scale);
void BSP_LCD_BacklightOn(void);
void BSP_LCD_BacklightOff(void);

#endif

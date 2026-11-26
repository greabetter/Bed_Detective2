/*
 * LCD.c
 *
 *  Created on: Nov 23, 2026
 *      Author: 31204
 */
/* ---- 引脚宏 ---- */
#include "LCD.h"


#define CS_L()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define CS_H()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define DC_CMD()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET)
#define DC_DATA() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)
#define RST_L()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET)
#define RST_H()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi1;

static void LCD_WriteCmd(uint8_t cmd)
{
    DC_CMD();
    CS_L();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    CS_H();
}

static void LCD_WriteData(const uint8_t *buf, uint16_t len)
{
    DC_DATA();
    CS_L();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, 1000);
    CS_H();
}

void LCD_Init(void)
{
    RST_H();
    HAL_Delay(10);
    RST_L();
    HAL_Delay(20);
    RST_H();
    HAL_Delay(120);

    LCD_WriteCmd(0x01);          /* 软件复位 */
    HAL_Delay(120);
    LCD_WriteCmd(0x11);          /* 退出睡眠 */
    HAL_Delay(120);

    LCD_WriteCmd(0x3A);          /* 像素格式 */
    { uint8_t d = 0x55; LCD_WriteData(&d, 1); }   /* 16bit RGB565 */

    LCD_WriteCmd(0x36);          /* 扫描方向 */
    { uint8_t d = 0x48; LCD_WriteData(&d, 1); }   /* MX + BGR，竖屏 */

    LCD_WriteCmd(0x29);          /* 开显示 */
    HAL_Delay(20);
}

void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t b[4];
    LCD_WriteCmd(0x2A);
    b[0]=x1>>8; b[1]=x1&0xFF; b[2]=x2>>8; b[3]=x2&0xFF;
    LCD_WriteData(b, 4);

    LCD_WriteCmd(0x2B);
    b[0]=y1>>8; b[1]=y1&0xFF; b[2]=y2>>8; b[3]=y2&0xFF;
    LCD_WriteData(b, 4);

    LCD_WriteCmd(0x2C);   /* 写显存 */
}

void LCD_Fill(uint16_t color)
{
    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint8_t buf[512];
    for (int i = 0; i < 512; i += 2) { buf[i] = hi; buf[i+1] = lo; }

    LCD_SetWindow(0, 0, 239, 319);
    DC_DATA(); CS_L();
    for (int i = 0; i < 240 * 320 * 2 / 512; i++)
        HAL_SPI_Transmit(&hspi1, buf, 512, 1000);
    CS_H();
}


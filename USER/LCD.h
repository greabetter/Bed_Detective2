/*
 * LCD.h
 *
 *  Created on: Nov 23, 2026
 *      Author: 31204
 */

#ifndef LCD_H_
#define LCD_H_

#include "main.h"

static void LCD_WriteCmd(uint8_t cmd);
static void LCD_WriteData(const uint8_t *buf, uint16_t len);
void LCD_Init(void);
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Fill(uint16_t color);

#endif /* LCD_H_ */

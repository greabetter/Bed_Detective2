/*
 * ft6336.h
 *
 *  Created on: Nov 27, 2026
 *      Author: 31204
 */

#ifndef TOUCH_FT6336_H_
#define TOUCH_FT6336_H_

#include <stdbool.h>
#include <stdint.h>

bool ft6336_init(void);
bool ft6336_read(uint16_t *x, uint16_t *y);
void ft6336_indev_init(void);      /* 注册给 LVGL */

#endif /* TOUCH_FT6336_H_ */

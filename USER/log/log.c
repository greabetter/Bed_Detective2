/*
 * log.c
 *
 *  Created on: Nov 27, 2026
 *      Author: 31204
 */


#include "log.h"
#include "main.h"
#include "lvgl.h"

#include <stdarg.h>
#include <string.h>

extern UART_HandleTypeDef huart1;      /* 句柄定义在 main.c */

void log_fmt(const char *fmt, ...)
{
    char buf[128];
    va_list ap;

    va_start(ap, fmt);
    lv_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
}

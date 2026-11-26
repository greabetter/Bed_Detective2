/*
 * lv_port.c
 *
 *  Created on: Nov 25, 2026
 *      Author: 31204
 */


#include "lv_port.h"
#include "lvgl.h"
#include "LCD.h"

extern SPI_HandleTypeDef hspi1;

#define CS_L()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define CS_H()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define DC_CMD()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET)
#define DC_DATA() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)

static lv_display_t * disp;
static uint8_t draw_buf[240 * 40 * 2];   /* 19.2 KB */

static void disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    LCD_SetWindow(area->x1, area->y1, area->x2, area->y2);

    DC_DATA();
    CS_L();
    HAL_SPI_Transmit(&hspi1, px_map, w*h*2, 1000);
    CS_H();

    lv_display_flush_ready(d);      /* 阻塞版：发完立刻通知 */
}

void lv_port_init(void)
{
    lv_init();
    lv_tick_set_cb(HAL_GetTick);    /* 用 HAL 的毫秒计数当 tick */
    disp = lv_display_create(240, 320);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);   /* ← 关键 */
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

}


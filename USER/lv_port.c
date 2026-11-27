#include "lv_port.h"
#include "lvgl.h"
#include "LCD.h"
#include "log.h"

extern SPI_HandleTypeDef hspi1;

#define CS_L()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define CS_H()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define DC_DATA() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)

#define LCD_W  320
#define LCD_H  480

static lv_display_t * disp;
static uint8_t draw_buf[LCD_W * 40 * 2];   /* 320×40×2 = 25600 字节 */

static void disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    LOG("F1 %d,%d %dx%d\r\n", area->x1, area->y1, w, h);
    LCD_SetWindow(area->x1, area->y1, area->x2, area->y2);
    LOG("F2\r\n");
    DC_DATA();
    CS_L();
    HAL_SPI_Transmit(&hspi1, px_map, w * h * 2, 1000);
    CS_H();
    LOG("F3\r\n");
    lv_display_flush_ready(d);
}

static void lv_log_cb(lv_log_level_t level, const char *buf)
{
    LOG("[LV] %s", buf);
}

void lv_port_init(void)
{
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}


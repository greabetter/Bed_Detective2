/*
 * ft6336.c
 *
 *  Created on: Nov 27, 2026
 *      Author: 31204
 */


#include "ft6336.h"
#include "main.h"
#include "lvgl.h"

extern I2C_HandleTypeDef hi2c1;

/* ---------- 配置 ---------- */
#define FT_ADDR        (0x38 << 1)      /* 7 位地址 0x38 */
#define FT_RST_PORT    GPIOC
#define FT_RST_PIN     GPIO_PIN_5

#define FT_REG_TD_STATUS  0x02
#define FT_REG_P1_XH      0x03

/* 坐标修正开关：方向不对就改这里 */
#define TOUCH_SWAP_XY     0
#define TOUCH_MIRROR_X    0
#define TOUCH_MIRROR_Y    0
#define TOUCH_W           320
#define TOUCH_H           480

/* ---------- 底层读 ---------- */
static bool ft_reg_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, FT_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, 100) == HAL_OK;
}

/* ---------- 初始化 ---------- */
bool ft6336_init(void)
{
    /* 硬复位：低 10ms → 高 → 等 300ms */
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(300);

    uint8_t id = 0;
    if (!ft_reg_read(0xA3, &id, 1)) {
        return false;          /* I2C 不通 */
    }
    return true;
}

/* ---------- 读一个触摸点 ---------- */
/* 读一个触摸点，带范围校验 */
bool ft6336_read(uint16_t *x, uint16_t *y)
{
    uint8_t buf[5];

    if (!ft_reg_read(FT_REG_TD_STATUS, buf, 5)) return false;

    uint8_t n = buf[0] & 0x0F;

    /* 1. 点数必须是 1（0=无触摸，>2 是异常） */
    if (n == 0 || n > 2) return false;

    uint16_t tx = ((buf[1] & 0x0F) << 8) | buf[2];
    uint16_t ty = ((buf[3] & 0x0F) << 8) | buf[4];

    /* 2. 坐标必须在屏幕范围内，否则丢弃 */
    if (tx >= TOUCH_W || ty >= TOUCH_H) return false;

    /* 3. 坐标 (0,0) 多半是无效值 */
    if (tx == 0 && ty == 0) return false;

    *x = tx;
    *y = ty;
    return true;
}

/* ---------- 给 LVGL 的输入回调 ---------- */
static void indev_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    static uint16_t last_x = 0, last_y = 0;
    static uint8_t  on_cnt = 0, off_cnt = 0;
    static bool     pressed = false;
    uint16_t x, y;

    if (ft6336_read(&x, &y)) {
        if (on_cnt < 3) on_cnt++;
        off_cnt = 0;
        last_x = x;
        last_y = y;
        if (on_cnt >= 3) pressed = true;      /* 连续 3 次才认为按下 */
    } else {
        if (off_cnt < 3) off_cnt++;
        on_cnt = 0;
        if (off_cnt >= 2) pressed = false;    /* 连续 2 次才认为松开 */
    }

    data->point.x = last_x;
    data->point.y = last_y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void ft6336_indev_init(void)
{
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_cb);
}

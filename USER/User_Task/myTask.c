/*
 * Task.c
 *
 *  Created on: Nov 25, 2026
 *      Author: 31204
 */
#include "myTask.h"
#include <stdio.h>
#include "main.h"          /* 已包含 HAL，RTC 类型和 HAL_RTC_* 函数都在里面 */

extern RTC_HandleTypeDef hrtc;   /* 句柄定义在 main.c，这里声明一下 */
/* ---------- 颜色 ---------- */
#define COLOR_OK      lv_color_hex(0x00C853)   /* 绿 */
#define COLOR_WARN    lv_color_hex(0xFFB300)   /* 黄 */
#define COLOR_ALARM   lv_color_hex(0xD50000)   /* 红 */
#define COLOR_DIM     lv_color_hex(0x9E9E9E)   /* 灰 */

/* ---------- 控件句柄 ---------- */
static lv_obj_t * bar;          /* 状态条 */
static lv_obj_t * led;          /* 状态灯 */
static lv_obj_t * lbl_status;   /* "正常" / "注意" / "报警" */
static lv_obj_t * lbl_time;     /* 时钟 */
static lv_obj_t * lbl_spo2;
static lv_obj_t * lbl_hr;
static lv_obj_t * lbl_rr;
static lv_obj_t * lbl_temp;

/* ========== 状态条 ========== */
static void status_bar_create(lv_obj_t * scr)
{
    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, 32);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_OK, 0);

    led = lv_led_create(bar);
    lv_obj_set_size(led, 14, 14);
    lv_obj_set_pos(led, 8, 9);
    lv_led_set_color(led, lv_color_white());
    lv_led_on(led);

    lbl_status = lv_label_create(bar);
    lv_label_set_text(lbl_status, "NORMAL");
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
    lv_obj_set_pos(lbl_status, 30, 8);

    lbl_time = lv_label_create(bar);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_RIGHT, -8, 8);
}

/* ========== 一行体征 ========== */
static lv_obj_t * row_create(lv_obj_t * scr, const char * name,
                             const char * unit, int32_t y,
                             const lv_font_t * val_font)
{
    lv_obj_t * n = lv_label_create(scr);
    lv_label_set_text(n, name);
    lv_obj_set_style_text_color(n, COLOR_DIM, 0);
    lv_obj_set_pos(n, 12, y + 10);

    lv_obj_t * v = lv_label_create(scr);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_font(v, val_font, 0);
    lv_obj_set_style_text_color(v, lv_color_black(), 0);
    lv_obj_set_pos(v, 70, y);

    lv_obj_t * u = lv_label_create(scr);
    lv_label_set_text(u, unit);
    lv_obj_set_style_text_color(u, COLOR_DIM, 0);
    lv_obj_set_pos(u, 166, y + 10);

    return v;
}

/* ========== 按钮回调 ========== */
static void trend_btn_cb(lv_event_t * e)
{
    lv_obj_t * page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(page, lv_color_hex(0xF5F5F5), 0);

    lv_obj_t * l = lv_label_create(page);
    lv_label_set_text(l, "DETAIL");
    lv_obj_center(l);

    lv_screen_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

static void detail_btn_cb(lv_event_t * e)
{
    lv_obj_t * page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(page, lv_color_hex(0xF5F5F5), 0);

    lv_obj_t * l = lv_label_create(page);
    lv_label_set_text(l, "TREND");
    lv_obj_center(l);

    lv_screen_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

/* ========== 底部按钮 ========== */
static void buttons_create(lv_obj_t * scr)
{
    lv_obj_t * b1 = lv_button_create(scr);
    lv_obj_set_size(b1, 104, 40);
    lv_obj_set_pos(b1, 10, 262);
    lv_obj_add_event_cb(b1, detail_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * t1 = lv_label_create(b1);
    lv_label_set_text(t1, "DETAIL");
    lv_obj_center(t1);

    lv_obj_t * b2 = lv_button_create(scr);
    lv_obj_set_size(b2, 104, 40);
    lv_obj_set_pos(b2, 126, 262);
    lv_obj_add_event_cb(b2, trend_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * t2 = lv_label_create(b2);
    lv_label_set_text(t2, "TREND");
    lv_obj_center(t2);
}

/* ========== 时钟 ========== */
static void clock_cb(lv_timer_t *t)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);   /* ← 必须调，见下方说明 */

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", sTime.Hours, sTime.Minutes);
}


/* ========== 数据模拟 ========== */
static void sim_cb(lv_timer_t *t)
{
    vitals_t v = { 72 + rand()%8, 96 + rand()%4, 36.4f + (rand()%5)*0.1f,
                   14 + rand()%4, 0 };
    ui_update(&v);
}

/* ========== 初始化 ========== */
void ui_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lbl_spo2 = row_create(scr, "SpO2", "%",   40, &lv_font_montserrat_28);
    lbl_hr   = row_create(scr, "HR",   "bpm", 96, &lv_font_montserrat_28);

    /* 分隔线 */
    lv_obj_t * line = lv_obj_create(scr);
    lv_obj_set_size(line, 216, 1);
    lv_obj_set_pos(line, 12, 142);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_border_width(line, 0, 0);

    lbl_rr   = row_create(scr, "RR",   "/min", 152, &lv_font_montserrat_28);
    lbl_temp = row_create(scr, "TEMP", "C",    206, &lv_font_montserrat_28);

    buttons_create(scr);



    lv_timer_create(sim_cb, 500, NULL);
    lv_timer_create(clock_cb, 1000, NULL);
}





/* ========== 更新 ========== */
void ui_update(const vitals_t * v)
{
    if (v == NULL) return;

    /* 数值 */
    lv_label_set_text_fmt(lbl_spo2, "%d", v->spo2);
    lv_label_set_text_fmt(lbl_hr,   "%d", v->hr);
    lv_label_set_text_fmt(lbl_rr,   "%d", v->rr);
    lv_label_set_text_fmt(lbl_temp, "%.1f", (double)v->temp);

    /* 状态条 */
    lv_color_t c;
    const char * txt;

    switch (v->level) {
    case 2:  c = COLOR_ALARM; txt = "ALARM"; break;
    case 1:  c = COLOR_WARN;  txt = "WARNING"; break;
    default: c = COLOR_OK;    txt = "NORMAL"; break;
    }

    lv_obj_set_style_bg_color(bar, c, 0);
    lv_label_set_text(lbl_status, txt);

    /* 报警时数值变红 */
    lv_obj_set_style_text_color(lbl_spo2,
        (v->spo2 < 92) ? COLOR_ALARM : lv_color_black(), 0);
    lv_obj_set_style_text_color(lbl_hr,
        (v->hr > 120 || v->hr < 50) ? COLOR_ALARM : lv_color_black(), 0);
}



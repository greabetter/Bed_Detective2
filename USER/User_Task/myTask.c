/*
 * myTask.c  —  ST7796 320x480 + FT6336U 触摸
 */

#include "myTask.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "main.h"
#include "log.h"

extern RTC_HandleTypeDef hrtc;
#define DBG_LED_TOGGLE()   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1)
/* ================= 屏幕与字体 ================= */
#define UI_W            320
#define UI_H            480

#define FONT_VALUE      (&lv_font_montserrat_28)
#define FONT_TEXT       (&lv_font_montserrat_14)

/* ================= 颜色 ================= */
#define COLOR_OK        lv_color_hex(0x00C853)
#define COLOR_WARN      lv_color_hex(0xFFB300)
#define COLOR_ALARM     lv_color_hex(0xD50000)
#define COLOR_DIM       lv_color_hex(0x757575)
#define COLOR_BG        lv_color_hex(0xF5F5F5)
#define COLOR_LINE      lv_color_hex(0xE0E0E0)

/* ================= 控件句柄 ================= */
static lv_obj_t * main_scr = NULL;   /* 主屏，永不删除 */
static lv_obj_t * bar;
static lv_obj_t * led;
static lv_obj_t * lbl_status;
static lv_obj_t * lbl_time;
static lv_obj_t * lbl_spo2;
static lv_obj_t * lbl_hr;
static lv_obj_t * lbl_rr;
static lv_obj_t * lbl_temp;

static uint8_t g_level = 0;
void lv_assert_failed(void)
{
    LOG("ASSERT RA=0x%x\r\n",
        (unsigned int)(uintptr_t)__builtin_return_address(0));
    while (1) { }
}
/* ================= 状态条 ================= */
static void status_bar_create(lv_obj_t * scr)
{
    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, UI_W, 48);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_OK, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    led = lv_led_create(bar);
    lv_obj_set_size(led, 20, 20);
    lv_obj_set_pos(led, 14, 14);
    lv_led_set_color(led, lv_color_white());
    lv_led_on(led);

    lbl_status = lv_label_create(bar);
    lv_label_set_text(lbl_status, "NORMAL");
    lv_obj_set_style_text_font(lbl_status, FONT_TEXT, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
    lv_obj_set_pos(lbl_status, 46, 16);

    lbl_time = lv_label_create(bar);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_font(lbl_time, FONT_TEXT, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_RIGHT, -14, 16);
}

/* ================= 一行体征 ================= */
static lv_obj_t * row_create(lv_obj_t * scr, const char * name,
                             const char * unit, int32_t y)
{
    lv_obj_t * n = lv_label_create(scr);
    lv_label_set_text(n, name);
    lv_obj_set_style_text_font(n, FONT_TEXT, 0);
    lv_obj_set_style_text_color(n, COLOR_DIM, 0);
    lv_obj_set_pos(n, 20, y + 12);

    lv_obj_t * v = lv_label_create(scr);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_font(v, FONT_VALUE, 0);
    lv_obj_set_style_text_color(v, lv_color_black(), 0);
    lv_obj_set_pos(v, 110, y);

    lv_obj_t * u = lv_label_create(scr);
    lv_label_set_text(u, unit);
    lv_obj_set_style_text_font(u, FONT_TEXT, 0);
    lv_obj_set_style_text_color(u, COLOR_DIM, 0);
    lv_obj_set_pos(u, 248, y + 12);

    return v;
}

/* ================= 页面切换防重入 ================= */
static uint32_t last_nav_tick = 0;

static bool nav_allowed(void)
{
    uint32_t now = lv_tick_get();
    if (now - last_nav_tick < 400) return false;
    last_nav_tick = now;
    return true;
}

/* ================= 返回主屏 ================= */
static void back_btn_cb(lv_event_t * e)
{
    DBG_LED_TOGGLE();                 /* ← 每次进回调翻一次 */
    if (!nav_allowed()) return;
    if (main_scr == NULL) return;

    /* 回主屏，同时删除当前页 */
    lv_screen_load_anim(main_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

/* ================= 创建占位页 ================= */
static void make_todo_page(const char * text)
{
    lv_obj_t * prev = lv_screen_active();
    bool del_prev = (prev != main_scr);      /* 主屏不删，别的页切走即删 */

    lv_obj_t * page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(page, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l = lv_label_create(page);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, FONT_VALUE, 0);
    lv_obj_center(l);

    lv_obj_t * back = lv_button_create(page);
    lv_obj_set_size(back, 100, 50);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * bl = lv_label_create(back);
    lv_label_set_text(bl, "BACK");
    lv_obj_set_style_text_font(bl, FONT_TEXT, 0);
    lv_obj_center(bl);

    /* 无动画：SPI 屏全屏动画太慢，容易堆积触摸事件 */
    lv_screen_load_anim(page, LV_SCR_LOAD_ANIM_NONE, 0, 0, del_prev);
}

static void detail_btn_cb(lv_event_t * e)
{
    DBG_LED_TOGGLE();                 /* ← 每次进回调翻一次 */
    if (!nav_allowed()) return;
    make_todo_page("DETAIL PAGE (TODO)");
}

static void trend_btn_cb(lv_event_t * e)
{
    if (!nav_allowed()) return;
    make_todo_page("TREND PAGE (TODO)");
}

/* ================= 底部按钮 ================= */
static void buttons_create(lv_obj_t * scr)
{
    lv_obj_t * b1 = lv_button_create(scr);
    lv_obj_set_size(b1, 142, 68);
    lv_obj_set_pos(b1, 16, 392);
    lv_obj_add_event_cb(b1, detail_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * t1 = lv_label_create(b1);
    lv_label_set_text(t1, "DETAIL");
    lv_obj_set_style_text_font(t1, FONT_TEXT, 0);
    lv_obj_center(t1);

    lv_obj_t * b2 = lv_button_create(scr);
    lv_obj_set_size(b2, 142, 68);
    lv_obj_set_pos(b2, 162, 392);
    lv_obj_add_event_cb(b2, trend_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * t2 = lv_label_create(b2);
    lv_label_set_text(t2, "TREND");
    lv_obj_set_style_text_font(t2, FONT_TEXT, 0);
    lv_obj_center(t2);
}

/* ================= 时钟 ================= */
static void clock_cb(lv_timer_t * t)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    lv_label_set_text_fmt(lbl_time, "%02d:%02d", sTime.Hours, sTime.Minutes);
}

/* ================= 假数据（接传感器后删掉这个定时器） ================= */
static void sim_cb(lv_timer_t * t)
{
    vitals_t v;
    v.hr    = 72 + rand() % 8;
    v.spo2  = 96 + rand() % 4;
    v.temp  = 36.4f + (rand() % 5) * 0.1f;
    v.rr    = 14 + rand() % 4;
    v.level = 0;

    ui_update(&v);
}

/* ================= 初始化 ================= */
void ui_init(void)
{
    main_scr = lv_screen_active();          /* 记住主屏 */

    lv_obj_set_style_bg_color(main_scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(main_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(main_scr, 0, 0);
    lv_obj_set_style_border_width(main_scr, 0, 0);
    lv_obj_remove_flag(main_scr, LV_OBJ_FLAG_SCROLLABLE);

    status_bar_create(main_scr);

    lbl_spo2 = row_create(main_scr, "SpO2", "%",    64);
    lbl_hr   = row_create(main_scr, "HR",   "bpm", 148);

    lv_obj_t * line = lv_obj_create(main_scr);
    lv_obj_set_size(line, 280, 2);
    lv_obj_set_pos(line, 20, 212);
    lv_obj_set_style_bg_color(line, COLOR_LINE, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);

    lbl_rr   = row_create(main_scr, "RR",   "/min", 228);
    lbl_temp = row_create(main_scr, "TEMP", "C",    304);

    buttons_create(main_scr);

    srand(HAL_GetTick());

    lv_timer_create(sim_cb,   500,  NULL);
   lv_timer_create(clock_cb, 1000, NULL);
}




/* ================= 切屏判断 ================= */
static bool ui_is_main_active(void)
{
    return lv_screen_active() == main_scr;
}
/* ================= 更新 ================= */
void ui_update(const vitals_t * v)
{
    if (v == NULL) return;
    if (!ui_is_main_active()) return;
    int t10 = (int)(v->temp * 10.0f + 0.5f);

    lv_label_set_text_fmt(lbl_spo2, "%d", v->spo2);
    lv_label_set_text_fmt(lbl_hr,   "%d", v->hr);
    lv_label_set_text_fmt(lbl_rr,   "%d", v->rr);
    lv_label_set_text_fmt(lbl_temp, "%d.%d", t10 / 10, t10 % 10);

    g_level = v->level;

    lv_color_t c;
    const char * txt;

    switch (g_level) {
    case 2:  c = COLOR_ALARM; txt = "ALARM";   break;
    case 1:  c = COLOR_WARN;  txt = "WARNING"; break;
    default: c = COLOR_OK;    txt = "NORMAL";  break;
    }

    lv_obj_set_style_bg_color(bar, c, 0);
    lv_label_set_text(lbl_status, txt);

    lv_obj_set_style_text_color(lbl_spo2,
        (v->spo2 < 92) ? COLOR_ALARM : lv_color_black(), 0);
    lv_obj_set_style_text_color(lbl_hr,
        (v->hr > 120 || v->hr < 50) ? COLOR_ALARM : lv_color_black(), 0);
    lv_obj_set_style_text_color(lbl_temp,
        (t10 > 375) ? COLOR_ALARM : lv_color_black(), 0);
}

/*
 * Task.c
 *
 *  Created on: Nov 25, 2026
 *      Author: 31204
 */
#include <myTask.h>
#include "FreeRTOS.h"


lv_obj_t * label;

void ui_update_cb(void *arg)
{
    if (label) lv_label_set_text(label, (const char *)arg);
}

void my_lv_btn1_cb()
{
	lv_async_call(ui_update_cb, (void *)"Hello LVGL");
}

void my_lv_user_widget(void)
{
	//设置边界
	lv_obj_t * scr = lv_screen_active();
	lv_obj_set_style_border_width(scr, 0,0 );
	lv_obj_set_style_pad_all(scr, 0, 0);
	//按钮创建
	lv_obj_t * btn1 = lv_btn_create(scr);
	lv_obj_set_size(btn1,50,50);
	lv_obj_set_pos(btn1, 95, 135);
	//创建标签
	label = lv_label_create(scr);
	lv_obj_set_size(label,100,50);
	lv_obj_set_pos(label,70,110);
}

void lv_task()
{
	my_lv_user_widget();
}



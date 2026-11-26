/*
 * Task.h
 *
 *  Created on: Nov 25, 2026
 *      Author: 31204
 */

#ifndef USER_TASK_MYTASK_H_
#define USER_TASK_MYTASK_H_

#include "lvgl.h"

/* 一条体征数据 */
typedef struct {
    uint16_t hr;      /* 心率 bpm */
    uint8_t  spo2;    /* 血氧 % */
    float    temp;    /* 体表温度 ℃ */
    uint16_t rr;      /* 呼吸 次/分，没有就填 0 */
    uint8_t  level;   /* 0=正常 1=注意 2=报警 */
} vitals_t;

void ui_init(void);
void ui_update(const vitals_t *v);


#endif /* USER_TASK_MYTASK_H_ */

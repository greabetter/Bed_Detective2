/*
 * key.h
 *
 *  Created on: Nov 26, 2026
 *      Author: 31204
 */

#ifndef KEYS_KEY_H_
#define KEYS_KEY_H_
#include "main.h"


//按钮结构体
typedef struct
{
	GPIO_PinState Previous;
	GPIO_PinState Current;

	GPIO_TypeDef *GPIO_Port;
	uint16_t GPIO_Pin;

	void (*ClickedCallback)(void);

}KeyHandle_TypeDef;

void KEY_Init();
uint8_t KEY_Scan();

#endif /* KEYS_KEY_H_ */

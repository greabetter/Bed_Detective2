/*
 * key.c
 *
 *  Created on: Nov 26, 2026
 *      Author: 31204
 */

#include "key.h"


static KeyHandle_TypeDef hKey1;


/*
 * @brief 该函数是对按钮的初始化
 * */
void KEY_Init()
{
	//KEY1
	hKey1.GPIO_Port = Key_1_GPIO_Port;
	hKey1.GPIO_Pin = Key_1_Pin;
	hKey1.Current = GPIO_PIN_SET;
	hKey1.Previous = GPIO_PIN_SET;
	hKey1.ClickedCallback = NULL;
}

/*
 * @brief 该函数为按钮的扫描
 * */
uint8_t KEY_Scan(void)
{
    uint8_t ret = 0;

    hKey1.Current = HAL_GPIO_ReadPin(hKey1.GPIO_Port, hKey1.GPIO_Pin);

    /* 上拉 + 按键接地：SET → RESET 表示按下（下降沿） */
    if (hKey1.Previous == GPIO_PIN_SET && hKey1.Current == GPIO_PIN_RESET)
    {
        ret = 1;
    }

    hKey1.Previous = hKey1.Current;   /* 记录本次状态，供下次比较 */

    return ret;
}

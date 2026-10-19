#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "stm32f10x.h"
#include "DHT11.h"

/*
 * 显示模块（应用层）
 *
 * 职责：把"数据"画到 OLED 上。main.c 只需要填一个 Display_Data_t，
 *       然后调用 Display_Render()，完全不需要关心坐标和像素。
 *       以后要加新传感器，只需要往 Display_Data_t 里加字段。
 *
 * 屏幕布局（128×64）：
 *   y=0        标题      Smoke Detector
 *   y=16       温度      Temp  +26.0 C
 *   y=32       湿度      Humi  065.0 %
 *   y=48       烟雾      Smoke 1234  Normal
 *   y=56       状态      Status: OK
 */

/*显示数据结构*********************/

typedef struct
{
	/*温湿度（来自 DHT11）*/
	uint8_t EnvValid;			/*1=数据有效，0=尚未读到有效数据，显示 --.- */
	uint8_t EnvStatus;			/*DHT11_Read() 的返回值，用于状态行*/
	int8_t  TempInt;			/*温度整数部分，单位℃*/
	uint8_t TempDec;			/*温度小数部分*/
	uint8_t HumiInt;			/*湿度整数部分，单位%RH*/
	uint8_t HumiDec;			/*湿度小数部分*/

	/*烟雾（来自 MQ2）*/
	uint8_t  SmokeValid;		/*1=数据有效*/
	uint16_t SmokeAdc;			/*ADC 原始值，范围 0~4095*/
	uint8_t  SmokeAlarm;		/*1=超过报警阈值*/
} Display_Data_t;

/*函数声明*********************/

void Display_Init(void);						/*初始化OLED并画好固定界面*/
void Display_Render(const Display_Data_t *Data);/*刷新全部数值并推送到屏幕*/

#endif

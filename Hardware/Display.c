#include "stm32f10x.h"
#include "OLED.h"
#include "Display.h"
#include "MQ2.h"		/*只是为了拿到 MQ2_MODE：模拟量模式显示数值，数字量模式只显示状态*/

/*界面布局参数（单位：像素）*********************/

#define UI_TITLE_Y			0						/*标题行纵坐标*/
#define UI_TEMP_Y			16						/*温度行纵坐标*/
#define UI_HUMI_Y			32						/*湿度行纵坐标*/
#define UI_SMOKE_Y			48						/*烟雾行纵坐标*/
#define UI_STATUS_Y			56						/*状态行纵坐标*/

#define UI_LABEL_X			0						/*标签横坐标*/
#define UI_VALUE_X			40						/*数值区起始横坐标*/
#define UI_DOT_X			(UI_VALUE_X + 24)		/*小数点横坐标*/
#define UI_DEC_X			(UI_VALUE_X + 32)		/*小数位横坐标*/
#define UI_UNIT_X			(UI_VALUE_X + 40)		/*单位横坐标*/

#define UI_SMOKE_VALUE_X	36						/*烟雾数值起始横坐标*/
#define UI_SMOKE_STATE_X	66						/*烟雾状态文字横坐标*/

/*私有函数声明*********************/

static void Display_ShowStatus(uint8_t Status);

/**
  * 函    数：显示模块初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：初始化OLED并画好固定不变的界面（标题、标签、单位），
  *           可变部分由 Display_Render 负责刷新
  */
void Display_Init(void)
{
	OLED_Init();													/*OLED初始化*/
	OLED_Clear();													/*清空显存*/

	OLED_ShowString(22, UI_TITLE_Y, "Smoke Detector", OLED_6X8);	/*标题，居中显示*/

	OLED_ShowString(UI_LABEL_X, UI_TEMP_Y, "Temp", OLED_8X16);		/*温度标签*/
	OLED_ShowChar(UI_UNIT_X, UI_TEMP_Y, 'C', OLED_8X16);			/*温度单位*/

	OLED_ShowString(UI_LABEL_X, UI_HUMI_Y, "Humi", OLED_8X16);		/*湿度标签*/
	OLED_ShowChar(UI_UNIT_X, UI_HUMI_Y, '%', OLED_8X16);			/*湿度单位*/

	OLED_ShowString(UI_LABEL_X, UI_SMOKE_Y, "Smoke", OLED_6X8);		/*烟雾标签*/

	OLED_ShowString(UI_LABEL_X, UI_STATUS_Y, "Status: --", OLED_6X8);/*状态行初始内容*/

	OLED_Update();													/*推送显存到屏幕*/
}

/**
  * 函    数：刷新显示
  * 参    数：Data 要显示的数据
  * 返 回 值：无
  * 说    明：只负责画图，不负责采集数据；函数内部会调用 OLED_Update()
  */
void Display_Render(const Display_Data_t *Data)
{
	/*----------- 温度行 -----------*/
	/*先把数值区清空，避免上一帧更长的内容残留（例如从 "-12.3" 变成 "--.-"）*/
	OLED_ClearArea(UI_VALUE_X, UI_TEMP_Y, UI_UNIT_X - UI_VALUE_X, 16);
	if (Data->EnvValid)
	{
		/*符号位 + 2位整数 + 小数点 + 1位小数，例：+26.0*/
		OLED_ShowSignedNum(UI_VALUE_X, UI_TEMP_Y, Data->TempInt, 2, OLED_8X16);
		OLED_ShowChar(UI_DOT_X, UI_TEMP_Y, '.', OLED_8X16);
		OLED_ShowNum(UI_DEC_X, UI_TEMP_Y, Data->TempDec, 1, OLED_8X16);
	}
	else
	{
		OLED_ShowString(UI_VALUE_X, UI_TEMP_Y, "--.-", OLED_8X16);
	}

	/*----------- 湿度行 -----------*/
	OLED_ClearArea(UI_VALUE_X, UI_HUMI_Y, UI_UNIT_X - UI_VALUE_X, 16);
	if (Data->EnvValid)
	{
		/*3位整数 + 小数点 + 1位小数，例：065.0*/
		OLED_ShowNum(UI_VALUE_X, UI_HUMI_Y, Data->HumiInt, 3, OLED_8X16);
		OLED_ShowChar(UI_DOT_X, UI_HUMI_Y, '.', OLED_8X16);
		OLED_ShowNum(UI_DEC_X, UI_HUMI_Y, Data->HumiDec, 1, OLED_8X16);
	}
	else
	{
		OLED_ShowString(UI_VALUE_X, UI_HUMI_Y, "--.-", OLED_8X16);
	}

	/*----------- 烟雾行 -----------*/
	OLED_ClearArea(UI_SMOKE_VALUE_X, UI_SMOKE_Y, 128 - UI_SMOKE_VALUE_X, 8);
	if (Data->SmokeValid)
	{
#if MQ2_MODE
		/*模拟量模式：显示 ADC 原始值。MQ-2 的 ppm 需要先用洁净空气标定 R0，
		  没有标定条件时显示原始值更诚实，不给不可信的数据*/
		OLED_ShowNum(UI_SMOKE_VALUE_X, UI_SMOKE_Y, Data->SmokeAdc, 4, OLED_6X8);

		if (Data->SmokeAlarm)
		{
			OLED_ShowString(UI_SMOKE_STATE_X, UI_SMOKE_Y, "ALARM!", OLED_6X8);
		}
		else
		{
			OLED_ShowString(UI_SMOKE_STATE_X, UI_SMOKE_Y, "Normal", OLED_6X8);
		}
#else
		/*数字量模式：只有开关量，没有数值可显示，直接显示状态*/
		if (Data->SmokeAlarm)
		{
			OLED_ShowString(UI_SMOKE_VALUE_X, UI_SMOKE_Y, "ALARM!", OLED_6X8);
		}
		else
		{
			OLED_ShowString(UI_SMOKE_VALUE_X, UI_SMOKE_Y, "Normal", OLED_6X8);
		}
#endif
	}
	else
	{
		OLED_ShowString(UI_SMOKE_VALUE_X, UI_SMOKE_Y, "----", OLED_6X8);
	}

	/*----------- 状态行 -----------*/
	Display_ShowStatus(Data->EnvStatus);

	/*把显存数组推送到OLED屏幕*/
	OLED_Update();
}

/**
  * 函    数：显示温湿度传感器的状态
  * 参    数：Status DHT11_Read() 的返回值
  * 返 回 值：无
  * 说    明：先清空整行再显示，避免上一次的长字符串残留
  */
static void Display_ShowStatus(uint8_t Status)
{
	OLED_ClearArea(0, UI_STATUS_Y, 128, 8);

	switch (Status)
	{
		case DHT11_OK:
			OLED_ShowString(0, UI_STATUS_Y, "Status: OK", OLED_6X8);
			break;

		case DHT11_ERR_NO_RESPONSE:				/*无响应：多为接线、供电、上拉电阻问题*/
			OLED_ShowString(0, UI_STATUS_Y, "Err: No response", OLED_6X8);
			break;

		case DHT11_ERR_TIMEOUT:					/*读取超时：时序被打断或传感器异常*/
			OLED_ShowString(0, UI_STATUS_Y, "Err: Timeout", OLED_6X8);
			break;

		case DHT11_ERR_CHECKSUM:				/*校验错误：数据不可信，多为时序不稳*/
			OLED_ShowString(0, UI_STATUS_Y, "Err: Checksum", OLED_6X8);
			break;

		default:
			OLED_ShowString(0, UI_STATUS_Y, "Err: Unknown", OLED_6X8);
			break;
	}
}

#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Display.h"
#include "DHT11.h"
#include "MQ2.h"
#include "LED.h"
#include "Buzzer.h"
#include "Serial.h"

/*
 * 程序功能：烟雾 + 温湿度 监测报警
 *   OLED  ：0.96寸 4针脚I2C接口，SCL=PB8，SDA=PB9
 *   DHT11 ：DATA=PB1，VCC=3.3V
 *   MQ2   ：AO=PA0（ADC1_IN0，5V供电时必须分压），DO=PA1（本工程用模拟量模式）
 *   LED   ：板载LED PC13（超标常亮）
 *   蜂鸣器：PB12（超标长鸣）
 *   串口  ：USART1，PA9=TX / PA10=RX，9600-8-N-1，每个采集周期上报一行
 *
 * 报警逻辑：烟雾ADC值超过阈值 → 蜂鸣器长鸣 + LED常亮；
 *           降回阈值以下 → 蜂鸣器和LED都关闭。
 *           阈值在 MQ2.h 的 MQ2_ALARM_THRESHOLD 里改。
 *
 * 本文件只负责"采集数据 → 判断报警 → 填结构体"，显示相关的代码在 Display 模块里。
 */

/*报警回差：防止ADC值正好卡在阈值附近来回抖动，导致报警反复开关*/
/*超过阈值就报警，要降到 (阈值 - 回差) 以下才解除；设为0就是不用回差*/
#define ALARM_HYSTERESIS	100

/*全局变量*********************/

static DHT11_Data_t DHT11_Data;			/*DHT11的原始读取结果*/
static Display_Data_t Display_Data;		/*交给显示模块的数据*/
static uint8_t AlarmState = 0;			/*当前报警状态，1=正在报警*/

/**
  * 函    数：串口上报一次采集结果
  * 参    数：无
  * 返 回 值：无
  * 说    明：每个采集周期输出一行，方便用串口助手（9600,N,8,1）观察趋势和排查问题
  *           正常输出：   #12 T=26.0C H=65.0% Smoke=1234(thr=1500) Normal
  *           报警输出：   #13 T=26.1C H=64.0% Smoke=1702(thr=1500) ALARM
  *           读取失败：   #14 T=--.-C H=--.-% Smoke=1702(thr=1500) ALARM DHT11_ERR=1
  * 注意事项：1. Serial_Printf 内部缓冲区只有100字节，单行输出不要超过这个长度
  *           2. 串口是查询方式发送，9600bps 下每字节约1ms，整行约60ms，会占用主循环时间
  */
static void Serial_Report(void)
{
	static uint16_t Count = 0;					/*上报序号，方便核对采集周期有没有丢*/
	const char *AlarmStr = "Normal";			/*报警状态文字*/

	Count ++;
	if (Display_Data.SmokeAlarm)
	{
		AlarmStr = "ALARM";
	}

	/*温湿度：只有读到有效数据才打印数值，没读到就打占位符，避免把上一次的旧值当成新值*/
	if (Display_Data.EnvValid)
	{
		Serial_Printf("#%d T=%d.%dC H=%d.%d%% Smoke=%d(thr=%d) %s",
		              (int)Count,
		              (int)Display_Data.TempInt, (int)Display_Data.TempDec,
		              (int)Display_Data.HumiInt, (int)Display_Data.HumiDec,
		              (int)Display_Data.SmokeAdc, (int)MQ2_ALARM_THRESHOLD,
		              AlarmStr);
	}
	else
	{
		Serial_Printf("#%d T=--.-C H=--.-%% Smoke=%d(thr=%d) %s",
		              (int)Count,
		              (int)Display_Data.SmokeAdc, (int)MQ2_ALARM_THRESHOLD,
		              AlarmStr);
	}

	/*温湿度读取失败时把状态码追加在行尾，对照 DHT11.h 的返回值定义就能知道失败原因*/
	if (Display_Data.EnvStatus != DHT11_OK)
	{
		Serial_Printf(" DHT11_ERR=%d", (int)Display_Data.EnvStatus);
	}

	Serial_Printf("\r\n");
}

/**
  * 函    数：主函数
  * 参    数：无
  * 返 回 值：无
  * 说    明：初始化显示、传感器和报警输出，然后循环采集、判断、刷新，周期1.5s
  */
int main(void)
{
	uint8_t Status;

	Display_Init();						/*显示模块初始化：OLED初始化 + 画好固定界面*/
	DHT11_Init();						/*DHT11初始化，内部等待约1s上电稳定*/
	MQ2_Init();							/*MQ2初始化，内部等待预热*/
	LED_Init();							/*LED初始化，默认熄灭*/
	Buzzer_Init();						/*蜂鸣器初始化，默认不响*/
	Serial_Init();						/*串口初始化，USART1 9600-8-N-1，仅发送不再关心接收标志*/

	while (1)
	{
		/*1.采集温湿度*/
		Status = DHT11_Read(&DHT11_Data);
		Display_Data.EnvStatus = Status;			/*状态行始终显示本次读取结果*/

		if (Status == DHT11_OK)
		{
			Display_Data.EnvValid = 1;
			Display_Data.TempInt  = DHT11_Data.TempInt;
			Display_Data.TempDec  = DHT11_Data.TempDec;
			Display_Data.HumiInt  = DHT11_Data.HumiInt;
			Display_Data.HumiDec  = DHT11_Data.HumiDec;
		}
		/*读取失败时保留上一次的有效数据不刷新，只靠状态行提示错误*/

		/*2.采集烟雾*/
		Display_Data.SmokeAdc   = MQ2_Data();
		Display_Data.SmokeValid = 1;

		/*3.判断报警状态（带回差，避免阈值附近反复开关）*/
		if (AlarmState)
		{
			/*已经在报警：要降到 阈值-回差 以下才解除*/
			if (Display_Data.SmokeAdc + ALARM_HYSTERESIS < MQ2_ALARM_THRESHOLD)
			{
				AlarmState = 0;
			}
		}
		else
		{
			/*没在报警：超过阈值就进入报警*/
			if (MQ2_IsAlarm(Display_Data.SmokeAdc))
			{
				AlarmState = 1;
			}
		}

		Display_Data.SmokeAlarm = AlarmState;		/*交给显示模块显示 Normal / ALARM! */

		/*4.报警输出：超标就蜂鸣器长鸣 + LED常亮，否则都关闭*/
		if (AlarmState)
		{
			LED_On();
			Buzzer_On();
		}
		else
		{
			LED_Off();
			Buzzer_Off();
		}

		/*5.串口上报本次采集结果*/
		Serial_Report();

		/*6.刷新显示（函数内部会调用OLED_Update把显存推送到屏幕）*/
		Display_Render(&Display_Data);

		Delay_ms(1500);					/*DHT11采样周期不能小于1s，这里取1.5s*/
	}
}

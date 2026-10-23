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

/*上报周期(ms)：手机按 P 在这两个值之间切换*/
#define REPORT_PERIOD_FAST	1500		/*DHT11采样周期不能小于1s*/
#define REPORT_PERIOD_SLOW	5000

/*全局变量*********************/

static DHT11_Data_t DHT11_Data;			/*DHT11的原始读取结果*/
static Display_Data_t Display_Data;		/*交给显示模块的数据*/
static uint8_t AlarmState = 0;			/*当前报警状态，1=正在报警*/
static uint8_t MuteFlag = 0;			/*1=静音（LED照常亮，蜂鸣器不响），手机按 M 切换*/
static uint16_t ReportPeriod = REPORT_PERIOD_FAST;	/*当前上报周期*/
static uint8_t ForceRefresh = 0;		/*1=手机点了"立即刷新"，提前结束本次等待*/

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

	/*再发一行给手机 APP 解析的机器可读格式：D,温度,湿度,烟雾ADC,阈值,报警*/
	/*APP 按逗号切分，字段顺序固定；温湿度没读到时发 --.-（APP 会显示成占位符）*/
	if (Display_Data.EnvValid)
	{
		Serial_Printf("D,%d.%d,%d.%d,%d,%d,%d\r\n",
		              (int)Display_Data.TempInt, (int)Display_Data.TempDec,
		              (int)Display_Data.HumiInt, (int)Display_Data.HumiDec,
		              (int)Display_Data.SmokeAdc, (int)MQ2_ALARM_THRESHOLD,
		              (int)Display_Data.SmokeAlarm);
	}
	else
	{
		Serial_Printf("D,--.-,--.-,%d,%d,%d\r\n",
		              (int)Display_Data.SmokeAdc, (int)MQ2_ALARM_THRESHOLD,
		              (int)Display_Data.SmokeAlarm);
	}
}

/**
  * 函    数：处理手机 App 发来的命令
  * 参    数：无
  * 返 回 值：无
  * 说    明：手机点按钮会发 "@X\r\n"（X 是命令字母）。Serial 模块负责收包，
  *           收完整一行后把内容放进 Serial_RxPacket 并把 Serial_RxFlag 置 1，
  *           这里取出来按首字母执行。加新按钮就在 switch 里加个 case，
  *           同时改手机 App 里那个按钮的"发送内容"。
  *             @Q=立即刷新  @M=静音  @R=恢复声音  @S=再发一帧  @P=切换上报周期
  */
static void App_HandleCommand(void)
{
	char Cmd;

	if (Serial_RxFlag == 0)
	{
		return;							/*没有待处理的命令*/
	}

	Cmd = Serial_RxPacket[0];			/*只认第一个字符，后面可以带参数暂不解析*/
	Serial_RxFlag = 0;					/*清标志，允许接收下一条*/

	switch (Cmd)
	{
		case 'Q':						/*立即刷新：提前结束本次等待，马上重新采集*/
			ForceRefresh = 1;
			Serial_Printf("ACK,Q\r\n");
			break;

		case 'M':						/*静音蜂鸣器，LED 照常按报警状态亮*/
			MuteFlag = 1;
			Serial_Printf("ACK,M\r\n");
			break;

		case 'R':						/*恢复声音*/
			MuteFlag = 0;
			Serial_Printf("ACK,R\r\n");
			break;

		case 'S':						/*把当前这帧数据再发一次，不重新采集*/
			Serial_Report();
			break;

		case 'P':						/*切换上报周期：快(1.5s) ↔ 慢(5s)*/
			if (ReportPeriod == REPORT_PERIOD_FAST)
			{
				ReportPeriod = REPORT_PERIOD_SLOW;
			}
			else
			{
				ReportPeriod = REPORT_PERIOD_FAST;
			}
			Serial_Printf("ACK,P,%d\r\n", (int)ReportPeriod);
			break;

		default:
			Serial_Printf("ERR,UNKNOWN\r\n");
			break;
	}
}

/**
  * 函    数：等待指定毫秒，等待期间照样响应手机命令
  * 参    数：Ms 要等待的毫秒数
  * 返 回 值：无
  * 说    明：把一大段 Delay_ms 切成 50ms 一小段，每小段之间取一次命令，
  *           这样手机上点按钮最多 50ms 就有反应，而不是等完整个上报周期
  */
static void WaitWithApp(uint16_t Ms)
{
	uint16_t Step;

	while (Ms > 0 && ForceRefresh == 0)
	{
		Step = (Ms > 50) ? 50 : Ms;		/*最后不足50ms就按剩下的等*/
		Delay_ms(Step);
		Ms -= Step;
		App_HandleCommand();			/*等待期间处理手机命令*/
	}
	ForceRefresh = 0;					/*消费掉"立即刷新"请求*/
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

		/*4.报警输出：超标就LED常亮；蜂鸣器是否响取决于手机上有没有点过"静音"*/
		if (AlarmState)
		{
			LED_On();
			if (MuteFlag == 0)
			{
				Buzzer_On();
			}
			else
			{
				Buzzer_Off();
			}
		}
		else
		{
			LED_Off();
			Buzzer_Off();
		}

		/*5.串口上报本次采集结果（先一行给人看，再一行给手机App解析）*/
		Serial_Report();

		/*6.刷新显示（函数内部会调用OLED_Update把显存推送到屏幕）*/
		Display_Render(&Display_Data);

		/*7.等到下一个周期，等待期间照样处理手机按钮，点按钮最多50ms就有反应*/
		WaitWithApp(ReportPeriod);
	}
}

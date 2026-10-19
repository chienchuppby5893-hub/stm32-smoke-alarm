#include "ADC.h"
#include "Delay.h"
#include "MQ2.h"
#include "stm32f10x.h"

/*私有函数声明*********************/

#if MQ2_MODE
static uint16_t MQ2_GetValue(void);
#endif

/**
  * 函    数：MQ2初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：根据 MQ2_MODE 选择模拟量模式或数字量模式配置引脚，
  *           最后等待 MQ2_WARMUP_MS 毫秒，让传感器预热稳定
  */
void MQ2_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

#if MQ2_MODE
	RCC_APB2PeriphClockCmd(MQ2_AO_CLK, ENABLE);			/*使能GPIOA时钟*/

	GPIO_InitStructure.GPIO_Pin = MQ2_AO_PIN;			/*PA0*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		/*模拟输入*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	/*输入模式下无效，但必须赋值*/
	GPIO_Init(MQ2_AO_PORT, &GPIO_InitStructure);

	ADCX_Init();										/*初始化ADC1*/
#else
	RCC_APB2PeriphClockCmd(MQ2_DO_CLK, ENABLE);			/*使能GPIOA时钟*/

	GPIO_InitStructure.GPIO_Pin = MQ2_DO_PIN;			/*PA1*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		/*上拉输入*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	/*输入模式下无效，但必须赋值*/
	GPIO_Init(MQ2_DO_PORT, &GPIO_InitStructure);
#endif

	/*MQ-2 内部有加热丝，刚上电时读数会持续下降，必须先预热*/
	Delay_ms(MQ2_WARMUP_MS);
}

#if MQ2_MODE
/**
  * 函    数：读取一次ADC转换值
  * 参    数：无
  * 返 回 值：ADC转换值，范围0~4095
  * 说    明：私有函数，仅在模拟量模式下有效
  */
static uint16_t MQ2_GetValue(void)
{
	return ADC_GetValue(ADC_Channel_0, ADC_SampleTime_55Cycles5);
}
#endif

/**
  * 函    数：获取MQ2数据
  * 参    数：无
  * 返 回 值：模拟量模式下返回 MQ2_SAMPLE_NUM 次采样的平均值（0~4095）；
  *           数字量模式下返回引脚电平（0或1）
  * 说    明：累加用32位变量，避免采样次数调大后 uint16_t 溢出
  */
uint16_t MQ2_Data(void)
{
#if MQ2_MODE
	uint8_t i;
	uint32_t sum = 0;

	for (i = 0; i < MQ2_SAMPLE_NUM; i ++)
	{
		sum += MQ2_GetValue();
		Delay_ms(10);						/*留出ADC和传感器响应时间*/
	}

	return (uint16_t)(sum / MQ2_SAMPLE_NUM);
#else
	return (uint16_t)GPIO_ReadInputDataBit(MQ2_DO_PORT, MQ2_DO_PIN);
#endif
}

/**
  * 函    数：判断烟雾是否超过报警阈值
  * 参    数：AdcValue MQ2_Data() 的返回值
  * 返 回 值：1=超过阈值需要报警，0=正常
  */
uint8_t MQ2_IsAlarm(uint16_t AdcValue)
{
	return (AdcValue >= MQ2_ALARM_THRESHOLD) ? 1 : 0;
}

/**
  * 函    数：把ADC值换算成AO引脚电压
  * 参    数：无
  * 返 回 值：电压值，单位V
  * 说    明：STM32F103 的 ADC 参考电压是 VDDA，通常为 3.3V，不是 5V；
  *           若AO经过分压接入，还要乘回分压比 MQ2_DIVIDER_RATIO
  */
float MQ2_GetVoltage(void)
{
	return (float)MQ2_Data() * 3.3f / 4096.0f * MQ2_DIVIDER_RATIO;
}

/**
  * 函    数：粗略估算烟雾浓度（PPM）
  * 参    数：无
  * 返 回 值：估算的PPM值
  * 说    明：★这个换算未经标定，不能当作真实浓度★
  *           MQ-2 的 ppm 与 Rs/R0 是对数关系，且 R0 必须先用洁净空气标定；
  *           下面是"假设线性"的粗略公式，只适合观察变化趋势。
  *           没有标定条件时，界面上显示 ADC 原始值比显示 ppm 更诚实。
  */
float MQ2_GetData_PPM(void)
{
#if MQ2_MODE
	float voltage = MQ2_GetVoltage();
	float ppm = (voltage - 0.4f) * 1000.0f;

	if (ppm < 0.0f)
	{
		ppm = 0.0f;							/*PPM不能为负数*/
	}

	return ppm;
#else
	return (float)MQ2_Data();				/*数字量模式下返回0或1*/
#endif
}

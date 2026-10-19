#include "ADC.h"

/**
  * 函    数：ADC初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：配置ADC1的通道0为单次转换模式，软件触发，右对齐，独立模式
  */
void ADCX_Init(void)
{
    RCC_APB2PeriphClockCmd(ADC_CLK, ENABLE); //使能ADC1时钟
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); /*设置ADC分频因子为6，72M/6=12M，ADC最大时钟为14M*/

    ADC_RegularChannelConfig(ADC1,ADC_Channel_0,1,ADC_SampleTime_55Cycles5);//固定的12.5与55.5相加再/12兆赫兹即为需要等待的时间
	
	ADC_InitTypeDef ADC_InitStructure;//ADC初始化
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;//单次转换or连续转换 
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//数据右对齐
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;//软件触发转换
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;//独立模式
	ADC_InitStructure.ADC_NbrOfChannel = 1;//转换通道数目
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;//扫描模式
	ADC_Init(ADC1,&ADC_InitStructure);//初始化ADC1
	
	ADC_Cmd(ADC1,ENABLE);//使能ADC1
	
    /*复位校准*/
	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1) == SET);
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1) == SET);
}

/**
  * 函    数：获取ADC值
  * 参    数：channel ADC通道号，ADC_SampleTime采样时间
  * 返 回 值：返回ADC转换值；若等待转换结束超时则返回 0
  * 说    明：先配置通道和采样时间，再软件触发转换，等待转换结束后返回结果。
  *           原代码里的 while(ADC_GetFlagStatus(...) == RESET); 没有超时，
  *           ADC 时钟未使能或通道配置异常时程序会永久卡死，这里加了超时保护。
  *           注意：读数据寄存器(ADC_DR)会自动清除 EOC 标志，不需要手动清。
  */
uint16_t ADC_GetValue(uint8_t channel, uint8_t ADC_SampleTime)
{
	uint32_t timeout = 0;										/*超时计数*/

	ADC_RegularChannelConfig(ADCX,channel,1,ADC_SampleTime);	/*配置通道*/
	ADC_SoftwareStartConvCmd(ADCX,ENABLE);						/*软件触发转换*/

	while(ADC_GetFlagStatus(ADCX,ADC_FLAG_EOC) == RESET)		/*等待转换结束*/
	{
		if(++ timeout > 100000)									/*超时保护，防止ADC异常时死循环*/
		{
			return 0;
		}
	}

	return ADC_GetConversionValue(ADCX);						/*返回转换值*/
}
    

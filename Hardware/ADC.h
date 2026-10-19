#ifndef ADC_H
#define ADC_H
#include "stm32f10x.h"                  // Device header

/*ADC宏定义*/
#define ADC_CLK                   RCC_APB2Periph_ADC1
#define ADCX                      ADC1            

/*ADC通道号*/
void ADCX_Init(void);
uint16_t ADC_GetValue(uint8_t channel, uint8_t ADC_SampleTime);

#endif

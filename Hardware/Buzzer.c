#include "stm32f10x.h"
#include "Buzzer.h"

/**
  * 函    数：蜂鸣器初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：配置为推挽输出，并默认关闭，避免上电就响
  */
void Buzzer_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(BUZZER_CLK, ENABLE);		/*使能GPIOB时钟*/

	GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/*推挽输出*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

	Buzzer_Off();									/*默认不响*/
}

/**
  * 函    数：蜂鸣器响（长鸣）
  * 参    数：无
  * 返 回 值：无
  * 说    明：一直输出有效电平，直到调用 Buzzer_Off()
  */
void Buzzer_On(void)
{
	GPIO_WriteBit(BUZZER_PORT, BUZZER_PIN, (BitAction)BUZZER_ON_LEVEL);
}

/**
  * 函    数：蜂鸣器关闭
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Off(void)
{
	GPIO_WriteBit(BUZZER_PORT, BUZZER_PIN, (BitAction)(BUZZER_ON_LEVEL ? 0 : 1));
}

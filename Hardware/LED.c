#include "stm32f10x.h"
#include "LED.h"

/**
  * 函    数：LED初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：把报警用的LED配置为推挽输出，并默认熄灭，避免上电就亮
  */
void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/*----------- LED1：板载LED PC13 -----------*/
	RCC_APB2PeriphClockCmd(LED1_CLK, ENABLE);		/*使能GPIOC时钟*/

	GPIO_InitStructure.GPIO_Pin = LED1_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/*推挽输出*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;	/*PC13不得超过2MHz*/
	GPIO_Init(LED1_PORT, &GPIO_InitStructure);

#if LED2_ENABLE
	/*----------- LED2：PB14 -----------*/
	RCC_APB2PeriphClockCmd(LED2_CLK, ENABLE);		/*使能GPIOB时钟*/

	GPIO_InitStructure.GPIO_Pin = LED2_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(LED2_PORT, &GPIO_InitStructure);
#endif

	LED_Off();										/*默认熄灭*/
}

/**
  * 函    数：点亮LED
  * 参    数：无
  * 返 回 值：无
  */
void LED_On(void)
{
	GPIO_WriteBit(LED1_PORT, LED1_PIN, (BitAction)LED_ON_LEVEL);
#if LED2_ENABLE
	GPIO_WriteBit(LED2_PORT, LED2_PIN, (BitAction)LED_ON_LEVEL);
#endif
}

/**
  * 函    数：熄灭LED
  * 参    数：无
  * 返 回 值：无
  */
void LED_Off(void)
{
	GPIO_WriteBit(LED1_PORT, LED1_PIN, (BitAction)(LED_ON_LEVEL ? 0 : 1));
#if LED2_ENABLE
	GPIO_WriteBit(LED2_PORT, LED2_PIN, (BitAction)(LED_ON_LEVEL ? 0 : 1));
#endif
}

/**
  * 函    数：翻转LED状态
  * 参    数：无
  * 返 回 值：无
  * 说    明：本工程用的是"超标常亮"，暂时用不到这个函数，
  *           保留下来方便以后做闪烁报警
  */
void LED_Toggle(void)
{
	if (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == (uint8_t)LED_ON_LEVEL)
	{
		LED_Off();
	}
	else
	{
		LED_On();
	}
}

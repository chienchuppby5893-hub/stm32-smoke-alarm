#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/*
 * LED 驱动（只负责开/关）
 *
 */

#define LED2_ENABLE		1		/*1=使用第二个LED(PB14)，0=不用*/

/*点亮电平：0=低电平点亮（板载LED就是这样），1=高电平点亮*/
#define LED_ON_LEVEL	0

/*LED1：板载LED PC13*/
#define LED1_PORT		GPIOC
#define LED1_PIN		GPIO_Pin_13
#define LED1_CLK		RCC_APB2Periph_GPIOC

/*LED2：PB14*/
#define LED2_PORT		GPIOB
#define LED2_PIN		GPIO_Pin_14
#define LED2_CLK		RCC_APB2Periph_GPIOB

void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

#endif

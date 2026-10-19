#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

/*
 * 蜂鸣器驱动（只负责响/不响，不含任何报警判断逻辑）
 *
 * 接线：蜂鸣器模块 VCC → 3.3V 或 5V，GND → GND，I/O → PB12
 *
 * ⚠ 驱动能力：STM32 单个 IO 最大约 20~25mA，而蜂鸣器通常要 20~30mA，
 *   已经接近上限。江协的蜂鸣器模块自带三极管驱动，可以直接接 IO；
 *   如果是裸蜂鸣器，必须加三极管或 MOS 驱动，不能直连。
 *
 * ⚠ 有源还是无源：
 *   本驱动适用于「有源蜂鸣器」——内部自带振荡源，给直流电平就发声。
 *   如果是有源蜂鸣器，"长鸣"就是持续输出电平。
 *   如果是「无源蜂鸣器」，必须用 PWM 方波驱动（频率决定音调），本驱动不适用。
 *   判断方法：模块背面写 Active / 有源 就是有源。
 */

/*蜂鸣器引脚：PB12（江协示例同款，空闲且不与本工程其他功能冲突）*/
#define BUZZER_PORT			GPIOB
#define BUZZER_PIN			GPIO_Pin_12
#define BUZZER_CLK			RCC_APB2Periph_GPIOB

/*响的电平：0=低电平响（江协模块就是这样），1=高电平响*/
#define BUZZER_ON_LEVEL		0

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

#endif

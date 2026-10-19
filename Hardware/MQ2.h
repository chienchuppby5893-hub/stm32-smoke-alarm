#ifndef __MQ2_H
#define __MQ2_H
#include "stm32f10x.h"                  // Device header

/*
 * MQ-2 烟雾传感器驱动
 *
 * 接线：
 *   VCC ---- 5V（MQ-2 内部有加热丝，必须 5V，3.3V 加热不足会导致读数异常）
 *   GND ---- GND
 *   AO  ---- PA0（经分压后接入，见下方 MQ2_DIVIDER_RATIO 说明）
 *   DO  ---- PA8（数字量输出，只在 MQ2_MODE=0 时使用）
 *
 * ⚠ 电平安全（重要）：MQ-2 由 5V 供电时，AO/DO 输出电压最高可接近 5V，
 *   而 STM32 引脚在模拟输入(ADC)模式下绝对最大输入只有 VDDA(3.3V)+0.3V。
 *   直接接入会过压，轻则读数顶死在4095，重则打坏引脚。
 *
 *   查《STM32F103x8B数据手册》引脚定义表的 I/O structure 列可知：
 *     非5V容忍：PA0~PA7、PB0、PB1、PB5、PC0~PC5
 *     5V容忍(FT)：PA8~PA15、PB2~PB4、PB6~PB15、PC6~PC12
 *   注意：5V 容忍只在"数字输入"模式下成立；一旦配成模拟输入就不再容忍5V。
 *   所以 AO（模拟量）无论如何都要分压；DO（数字量）可以接 FT 引脚直接使用。
 *
 *   没有电阻时有两个零成本方案：
 *     方案A：把 VCC 改接 3.3V，AO/DO 自动不超过 3.3V，安全但灵敏度下降
 *     方案B：MQ2_MODE 改成 0，DO 接 PA8（5V容忍引脚），保持5V供电与灵敏度，
 *            代价是只能得到"正常/报警"，没有模拟量数值
 *   有电阻时推荐：20kΩ 串联 + 10kΩ 对地分压，5V → 1.67V 接 PA0，
 *                 此时 MQ2_DIVIDER_RATIO 填 3.0
 *
 * ⚠ 预热：MQ-2 上电后读数会持续漂移，需要预热一段时间才稳定，
 *   见 MQ2_WARMUP_MS。
 */

/*工作模式：1=模拟量模式（AO接ADC，本工程使用），0=数字量模式（DO接GPIO）*/
#define MQ2_MODE				1

/*模拟量模式下每次取平均的采样次数*/
#define MQ2_SAMPLE_NUM			10

/*报警阈值：ADC原始值，范围0~4095，需要按实际环境实测后调整*/
/*做法：预热后在洁净空气中观察ADC稳定值，阈值取比它明显高一些的值*/
#define MQ2_ALARM_THRESHOLD		1500

/*分压比：实际传感器电压 = MCU测得电压 × 本比值*/
/*例如 5V 经 20k:10k 分压接入，比值填 3.0；没加分压直接接入（不推荐）填 1.0*/
#define MQ2_DIVIDER_RATIO		1.0f

/*上电预热时间(ms)：MQ-2 需要预热读数才稳定，不需要可改为0*/
#define MQ2_WARMUP_MS			20000


/*引脚配置*********************/

#define MQ2_AO_PIN				GPIO_Pin_0				/*模拟量引脚*/
#define MQ2_DO_PIN				GPIO_Pin_8				/*数字量引脚：PA8是5V容忍引脚，可直接接5V的DO*/

#define MQ2_AO_PORT				GPIOA					/*模拟量端口*/
#define MQ2_DO_PORT				GPIOA					/*数字量端口*/

#define MQ2_AO_CLK				RCC_APB2Periph_GPIOA	/*模拟量端口时钟*/
#define MQ2_DO_CLK				RCC_APB2Periph_GPIOA	/*数字量端口时钟*/


/*函数声明*********************/

void     MQ2_Init(void);					/*初始化引脚并等待预热*/
uint16_t MQ2_Data(void);					/*取采样平均值，数字量模式下返回0或1*/
uint8_t  MQ2_IsAlarm(uint16_t AdcValue);	/*判断是否超过报警阈值*/
float    MQ2_GetVoltage(void);				/*把ADC值换算成AO引脚电压(V)*/
float    MQ2_GetData_PPM(void);				/*未标定的粗略换算，仅供调试，不要用于显示*/

#endif

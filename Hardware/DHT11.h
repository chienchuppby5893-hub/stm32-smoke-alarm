#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

/*
 * DHT11 温湿度传感器驱动（单总线，软件时序）
 *
 * 接线说明：
 *   DHT11 DATA ---- PB1（本文件 DHT11_DATA_PORT / DHT11_DATA_PIN 可改）
 *   DHT11 VCC  ---- 3.3V
 *   DHT11 GND  ---- GND
 *   建议在 DATA 与 VCC 之间接 4.7k~10k 上拉电阻
 *   （本驱动使用 GPIO 内部上拉，短导线可以工作，导线较长时必须加上拉电阻）
 *
 * 使用说明：
 *   1. 先调用 DHT11_Init()
 *   2. 然后循环调用 DHT11_Read()，两次读取的间隔不能小于 1s
 *   3. 通过返回值判断本次读取是否有效，失败时不要使用 Data 中的数据
 */

/*引脚配置*********************/

#define DHT11_DATA_PORT				GPIOB				/*DATA引脚所在端口*/
#define DHT11_DATA_PIN				GPIO_Pin_1			/*DATA引脚编号*/
#define DHT11_GPIO_CLK				RCC_APB2Periph_GPIOB	/*DATA引脚所在端口的时钟*/


/*时序参数*********************/

/*数据位高电平期间，延时到 40us 处采样：高电平 26~28us 表示 0，70us 表示 1*/
#define DHT11_SAMPLE_US				40
/*等待引脚电平跳变的最大次数（每次循环约 1us），用于防止传感器掉线时死循环*/
#define DHT11_BIT_TIMEOUT_US		100
/*等待从机响应信号的最大次数（每次循环约 1us）*/
#define DHT11_RESPONSE_TIMEOUT_US	100
/*DHT11上电后需要约 1s 的稳定时间才能正确读取，不需要可改为 0*/
#define DHT11_POWER_ON_DELAY_MS		1000


/*返回值定义*********************/

#define DHT11_OK					0		/*读取成功*/
#define DHT11_ERR_NO_RESPONSE		1		/*从机无响应：检查接线、供电、上拉电阻*/
#define DHT11_ERR_TIMEOUT			2		/*读取过程中等待电平跳变超时*/
#define DHT11_ERR_CHECKSUM			3		/*校验和错误，数据不可信*/


/*数据结构*********************/

/**
  * DHT11读取结果
  * DHT11（非DHT22）小数部分恒为 0；若换成 DHT22/AM2302，小数部分才有意义
  */
typedef struct
{
	uint8_t HumiInt;			/*湿度整数部分，单位 %RH，范围 0~100*/
	uint8_t HumiDec;			/*湿度小数部分，范围 0~9*/
	int8_t  TempInt;			/*温度整数部分，单位 ℃，范围 -40~+80*/
	uint8_t TempDec;			/*温度小数部分，范围 0~9*/
} DHT11_Data_t;


/*函数声明*********************/

void DHT11_Init(void);											/*初始化DATA引脚*/
void DHT11_Start(void);											/*主机发送起始信号*/
uint8_t DHT11_Check(void);										/*检测从机响应信号*/
uint8_t DHT11_Read_Byte(uint8_t *Byte);							/*接收一个字节*/
uint8_t DHT11_Read(DHT11_Data_t *Data);							/*读取一次温湿度（推荐）*/
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);			/*读取一次温湿度（兼容旧接口）*/

/*旧版本函数名拼写错误，保留此别名，原有代码不用改*/
#define DHT11_Satrt	DHT11_Start

#endif

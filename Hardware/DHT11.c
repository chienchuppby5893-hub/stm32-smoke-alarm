#include "stm32f10x.h"                  // Device header
#include "DHT11.h"
#include "Delay.h"

/*私有宏定义*********************/

/*读取DATA引脚电平，返回 0 或 1*/
#define DHT11_DATA_READ()	GPIO_ReadInputDataBit(DHT11_DATA_PORT, DHT11_DATA_PIN)
/*向DATA引脚写入电平，x 为 0 或 1*/
#define DHT11_DATA_WRITE(x)	GPIO_WriteBit(DHT11_DATA_PORT, DHT11_DATA_PIN, (BitAction)(x))

/*私有函数声明*********************/

static void DHT11_Mode(uint8_t Mode);
static uint8_t DHT11_Wait_Level(uint8_t Level, uint16_t TimeoutUs);

/**
  * 函    数：DHT11初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：只负责配置DATA引脚，并把总线释放为高电平（空闲状态）
  *           DHT11上电后需要约 1s 才能稳定，此处延时 DHT11_POWER_ON_DELAY_MS 毫秒
  */
void DHT11_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/*开启DATA引脚所在端口的时钟*/
	RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);

	/*空闲状态：引脚配置为上拉输入，即释放总线，由外部上拉拉为高电平*/
	GPIO_InitStructure.GPIO_Pin = DHT11_DATA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT11_DATA_PORT, &GPIO_InitStructure);

	/*预先将输出数据寄存器置 1，这样以后切换为推挽输出时，引脚立即是高电平*/
	GPIO_SetBits(DHT11_DATA_PORT, DHT11_DATA_PIN);

	/*等待传感器上电稳定*/
	#if (DHT11_POWER_ON_DELAY_MS > 0)
	Delay_ms(DHT11_POWER_ON_DELAY_MS);
	#endif
}

/**
  * 函    数：设置DATA引脚的方向
  * 参    数：Mode 1为推挽输出（主机驱动总线），0为上拉输入（主机释放总线）
  * 返 回 值：无
  */
static void DHT11_Mode(uint8_t Mode)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = DHT11_DATA_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		/*输入模式下此参数无效，但必须赋值*/
	GPIO_InitStructure.GPIO_Mode = (Mode != 0) ? GPIO_Mode_Out_PP : GPIO_Mode_IPU;

	GPIO_Init(DHT11_DATA_PORT, &GPIO_InitStructure);
}

/**
  * 函    数：等待DATA引脚变为指定电平
  * 参    数：Level 要等待的电平，0或1
  * 参    数：TimeoutUs 最大等待次数，每次循环约 1us
  * 返 回 值：DHT11_OK 表示等到指定电平，DHT11_ERR_TIMEOUT 表示超时
  * 说    明：内部用于替代原来的死循环 while(...)，传感器掉线时不会卡死程序
  */
static uint8_t DHT11_Wait_Level(uint8_t Level, uint16_t TimeoutUs)
{
	while (TimeoutUs --)
	{
		if (DHT11_DATA_READ() == Level)
		{
			return DHT11_OK;
		}
		Delay_us(1);
	}
	return DHT11_ERR_TIMEOUT;
}

/**
  * 函    数：主机发送起始信号
  * 参    数：无
  * 返 回 值：无
  * 说    明：主机拉低总线至少 18ms，再拉高 20~40us，然后释放总线等待从机响应
  */
void DHT11_Start(void)
{
	DHT11_Mode(1);							/*引脚切换为推挽输出*/
	DHT11_DATA_WRITE(0);					/*拉低总线*/
	Delay_ms(20);							/*保持至少 18ms，作为起始信号*/
	DHT11_DATA_WRITE(1);					/*拉高总线*/
	Delay_us(30);							/*保持 20~40us*/
	DHT11_Mode(0);							/*释放总线，等待从机响应*/
}

/**
  * 函    数：检测从机响应信号
  * 参    数：无
  * 返 回 值：DHT11_OK 从机响应正常，DHT11_ERR_NO_RESPONSE 从机无响应
  * 说    明：从机响应时会把总线拉低 80us，再拉高 80us，随后开始输出数据
  *           调用本函数后，总线正处于第 1 个数据位的低电平期间
  */
uint8_t DHT11_Check(void)
{
	if (DHT11_Wait_Level(0, DHT11_RESPONSE_TIMEOUT_US) != DHT11_OK) { return DHT11_ERR_NO_RESPONSE; }
	if (DHT11_Wait_Level(1, DHT11_RESPONSE_TIMEOUT_US) != DHT11_OK) { return DHT11_ERR_NO_RESPONSE; }
	if (DHT11_Wait_Level(0, DHT11_RESPONSE_TIMEOUT_US) != DHT11_OK) { return DHT11_ERR_NO_RESPONSE; }

	return DHT11_OK;
}

/**
  * 函    数：主机接收一个字节数据
  * 参    数：Byte 读取到的数据存放位置
  * 返 回 值：DHT11_OK 表示成功，其他值为错误码
  * 说    明：每一位数据都以 50us 低电平开始，随后高电平 26~28us 表示 0，70us 表示 1
  *           所以在延时 40us 后读取引脚电平，即可区分 0 和 1
  */
uint8_t DHT11_Read_Byte(uint8_t *Byte)
{
	uint8_t i;
	uint8_t Data = 0;
	uint8_t Status;

	for (i = 0; i < 8; i ++)
	{
		/*等待 50us 低电平结束，即数据位的开始*/
		Status = DHT11_Wait_Level(1, DHT11_BIT_TIMEOUT_US);
		if (Status != DHT11_OK) { return Status; }

		/*延时到高电平中间位置采样*/
		Delay_us(DHT11_SAMPLE_US);

		/*此时仍为高电平，说明数据位是 1，否则是 0*/
		if (DHT11_DATA_READ() == 1)
		{
			Data |= (uint8_t)(0x80 >> i);
		}

		/*等待本数据位的高电平结束，准备接收下一位*/
		/*说明：第40位结束后，从机是释放总线（保持高电平）还是再拉低50us，不同批次传感器不一致，
		        所以这里等待低电平超时属于正常情况，不作为错误处理，避免误判为读取失败；
		        真正读错的数据会在校验和那一步被剔除*/
		DHT11_Wait_Level(0, DHT11_BIT_TIMEOUT_US);
	}

	*Byte = Data;
	return DHT11_OK;
}

/**
  * 函    数：读取一次温湿度数据
  * 参    数：Data 读取结果存放位置
  * 返 回 值：DHT11_OK 表示成功，其他值为错误码，失败时 Data 中的数据无效
  * 说    明：一次完整的时序为：起始信号 -> 响应信号 -> 40位数据（5个字节）
  *           数据格式：湿度整数 湿度小数 温度整数 温度小数 校验和
  *           校验和 = 前4个字节之和的低8位
  *           本函数内部读数据期间会关闭中断，避免时序被中断打断
  *           两次调用本函数的间隔不能小于 1s
  */
uint8_t DHT11_Read(DHT11_Data_t *Data)
{
	uint8_t Buf[5] = {0};
	uint8_t i;
	uint8_t Status = DHT11_OK;
	uint32_t Primask;

	/*1.主机发送起始信号*/
	DHT11_Start();

	/*2.检测从机响应信号*/
	if (DHT11_Check() != DHT11_OK)
	{
		return DHT11_ERR_NO_RESPONSE;
	}

	/*3.连续接收40位数据，即5个字节*/
	/*关中断：单总线时序精度要求高，防止中断打断导致读取错误*/
	/*如果项目中不允许关中断，可将下面两行注释掉*/
	Primask = __get_PRIMASK();					/*保存中断开关状态*/
	__disable_irq();

	for (i = 0; i < 5; i ++)
	{
		Status = DHT11_Read_Byte(&Buf[i]);
		if (Status != DHT11_OK) { break; }
	}

	__set_PRIMASK(Primask);						/*恢复中断开关状态*/

	if (Status != DHT11_OK)
	{
		return Status;
	}

	/*4.校验：前4个字节之和的低8位应等于第5个字节*/
	if ((uint8_t)(Buf[0] + Buf[1] + Buf[2] + Buf[3]) != Buf[4])
	{
		return DHT11_ERR_CHECKSUM;
	}

	/*5.解析数据*/
	Data->HumiInt = Buf[0];
	Data->HumiDec = Buf[1];

	if ((Buf[3] & 0x80) != 0)					/*温度小数最高位为1表示负温度（DHT22）*/
	{
		Data->TempInt = -(int8_t)(Buf[2] & 0x7F);
		Data->TempDec = (uint8_t)(Buf[3] & 0x7F);
	}
	else
	{
		Data->TempInt = (int8_t)Buf[2];
		Data->TempDec = Buf[3];
	}

	return DHT11_OK;
}

/**
  * 函    数：读取一次温湿度数据（兼容旧接口，只有整数部分）
  * 参    数：temp 温度整数部分存放位置，不需要可传 0
  * 参    数：humi 湿度整数部分存放位置，不需要可传 0
  * 返 回 值：DHT11_OK 表示成功，其他值为错误码
  * 说    明：读取失败时不会修改 temp 和 humi，避免把错误数据当成真实温度显示出来
  */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
	DHT11_Data_t Data;
	uint8_t Status;

	Status = DHT11_Read(&Data);

	if (Status == DHT11_OK)
	{
		if (humi != 0) { *humi = Data.HumiInt; }
		if (temp != 0) { *temp = (uint8_t)Data.TempInt; }
	}

	return Status;
}

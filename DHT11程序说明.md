# DHT11 温湿度显示程序说明

工程位置：`2.1 OLED-V2.0\UTF-8\01-OLED功能函数测试-4针脚I2C接口`

## 一、硬件接线

| 器件 | 引脚 | 接到 STM32 |
| --- | --- | --- |
| OLED（0.96寸，4针脚 I2C） | SCL | PB8 |
| OLED | SDA | PB9 |
| OLED | VCC / GND | 3.3V / GND |
| DHT11 | DATA | **PB1** |
| DHT11 | VCC / GND | 3.3V / GND |

> DHT11 的 DATA 与 VCC 之间**建议接 4.7k~10k 上拉电阻**。
> 程序里用的是 GPIO 内部上拉（约 40kΩ），导线短的时候能用，导线一长就容易出现
> `Err: No response`。DHT11 模块（带小板）一般已经焊了上拉电阻，裸传感器必须自己加。

## 二、屏幕显示效果

```
          DHT11 Sensor
Temp    +26.0 C
Humi    065.0 %
Status: OK
```

- 温度带符号位，湿度固定 3 位整数
- 每 1.5s 刷新一次（DHT11 采样周期不能小于 1s）
- 读取失败时**保留上一次的有效数值**，只把状态行改成错误原因
- 从没读取成功过时，数值区显示 `--.-`，不会显示乱七八糟的数字

状态行含义：

| 显示 | 含义 | 排查方向 |
| --- | --- | --- |
| `Status: OK` | 读取成功 | — |
| `Err: No response` | 传感器没有响应 | 接线、供电、上拉电阻、引脚号写错 |
| `Err: Timeout` | 读取数据位超时 | 时序被打断、导线太长、传感器损坏 |
| `Err: Checksum` | 校验和错误 | 时序不稳（多为导线/上拉电阻问题） |

## 三、文件说明

| 文件 | 说明 |
| --- | --- |
| `Hardware/DHT11.h` | 引脚、时序参数、错误码、数据结构、函数声明 |
| `Hardware/DHT11.c` | 单总线驱动实现 |
| `User/main.c` | 主程序：界面绘制 + 循环读取 + 状态显示 |
| `Hardware/OLED.c` `OLED_Data.c` | 江协科技原版 OLED 驱动，未改动 |
| `System/Delay.c` | 江协科技原版延时函数，未改动 |

## 四、对外函数（API）

```c
void    DHT11_Init(void);                        /* 初始化 DATA 引脚，含上电稳定延时 */
uint8_t DHT11_Read(DHT11_Data_t *Data);          /* 读一次温湿度，推荐使用 */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);  /* 旧接口，只取整数部分 */
```

返回值：

```c
DHT11_OK               0   成功
DHT11_ERR_NO_RESPONSE  1   从机无响应
DHT11_ERR_TIMEOUT      2   读取超时
DHT11_ERR_CHECKSUM     3   校验和错误
```

数据结构（含小数部分，方便以后换 DHT22/AM2302）：

```c
typedef struct
{
    uint8_t HumiInt;    /* 湿度整数 0~100 %RH */
    uint8_t HumiDec;    /* 湿度小数 0~9，DHT11 恒为 0 */
    int8_t  TempInt;    /* 温度整数 -40~+80 ℃ */
    uint8_t TempDec;    /* 温度小数 0~9，DHT11 恒为 0 */
} DHT11_Data_t;
```

典型用法：

```c
DHT11_Data_t Data;

DHT11_Init();
while (1)
{
    if (DHT11_Read(&Data) == DHT11_OK)
    {
        /* 只有返回 OK 时 Data 里的数据才可信 */
    }
    Delay_ms(1500);
}
```

## 五、本次修改内容

原来的代码能跑，但有几个隐患，本次一并修好：

1. **`DHT11_Read_Byte` 里的 `while(...)` 是死循环**，没有超时。传感器拔掉或者接触不良时，
   程序会永久卡死在这几行里，OLED 再也不刷新。现已全部改为带超时的 `DHT11_Wait_Level()`。
2. **没有返回值**。原来 `DHT11_Read_Data()` 返回 `void`，读失败（比如校验和错误）时
   `temp`/`humi` 保持旧值不变，程序却不知道，会把错误数据当真实温度显示。
   现在返回错误码，失败时不再改写输出参数。
3. **`DHT11_Mode()` 里 `GPIO_Speed` 没赋值**。输入分支没有给 `GPIO_InitStructure.GPIO_Speed`
   赋值就调用了 `GPIO_Init()`，读的是未初始化的栈内存。现已两个分支都赋值。
4. **显示坐标错误**。原来 `OLED_ShowSignedNum(0, 0, ...)` 和 `OLED_ShowSignedNum(1, 0, ...)`
   的第二个参数是**像素纵坐标**，不是行号，1 和 0 只差一个像素，两行字会重叠在一起。
   现已改为 0 / 16 / 32 / 56 的正确坐标。
5. **`DHT11.h` 缺少 `#include "stm32f10x.h"`**，`u8`、`uint8_t`、`GPIO_Pin_1` 全靠 include
   顺序碰巧能用，一旦单独 include 就会报错。现已补上，并删掉用不到的 `<stdio.h>`。
6. **去掉了 `DHT11_Init()` 里的空读**。原来初始化时先 `DHT11_Satrt()` 再 `DHT11_Check()`，
   读到的数据直接丢掉，还多吃一次采样。现在初始化只配置引脚，读数据统一由 `DHT11_Read()` 负责。
7. 增加了小数部分、负温度（DHT22 兼容）、中断保护（读取 40 位数据期间关中断）、
   错误状态显示等。
8. `DHT11_Satrt` 拼写错误，新函数名为 `DHT11_Start`，同时保留 `#define DHT11_Satrt DHT11_Start`
   别名，老代码不用改。
9. 修正 `.eide/eide.yml` 中 `DHT11.h` 的路径（原来写成了根目录，正确是 `Hardware/DHT11.h`）。

## 六、常见问题

**Q：一直显示 `Err: No response`？**
1. 确认 DATA 接的是 PB1（或把 `DHT11.h` 里的 `DHT11_DATA_PIN` / `DHT11_DATA_PORT` /
   `DHT11_GPIO_CLK` 改成自己实际用的引脚）
2. 万用表量一下 DHT11 的 VCC 是不是 3.3V
3. DATA 与 VCC 之间补一个 4.7k 上拉电阻
4. 裸传感器和模块的引脚顺序不一样（裸传感器从左到右是 VCC/DATA/NC/GND），别接反

**Q：偶尔能读出来，偶尔 `Err: Checksum`？**
时序不稳，通常是导线太长或没加上拉电阻。也可以把 `DHT11.h` 里的 `DHT11_SAMPLE_US`
在 35~45 之间微调，或者确认 `Delay_us()` 用的是 72MHz 主频（本工程 SystemInit 默认 72MHz）。

**Q：想改成串口打印？**
本工程没有配置 USART，需要再加 USART 初始化和 `fputc` 重定向。需要的话可以继续加。

## 七、编译

用 Keil5 打开 `Project.uvprojx` 直接编译（工程里已经包含 `DHT11.c`，无需手动添加）。
也可以用 VS Code + EIDE 插件编译（`.eide/eide.yml` 已同步修正）。

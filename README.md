# STM32 烟雾 + 温湿度 监测报警系统

[![Release](https://img.shields.io/github/v/release/chienchuppby5893-hub/stm32-smoke-alarm)](https://github.com/chienchuppby5893-hub/stm32-smoke-alarm/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

基于 **STM32F103C8T6** + **标准外设库（StdPeriph）** 的环境监测小项目：
DHT11 采集温湿度、MQ-2 采集烟雾浓度、0.96 寸 OLED 实时显示，浓度超标时蜂鸣器长鸣 + LED 常亮。

## 功能

- **温湿度采集**：DHT11 单总线读取，1.5s 刷新一次（DHT11 采样周期不能小于 1s）
- **烟雾采集**：MQ-2 模拟量输出接 ADC1_IN0，每次取 10 次采样平均值
- **阈值报警**：ADC 值超过阈值 → 蜂鸣器长鸣 + LED 常亮；降回阈下 → 自动解除
- **报警回差**：回差 100，避免读数卡在阈值附近时报警反复开关
- **容错显示**：DHT11 读取失败时保留上一次有效数据，状态行提示错误，不会花屏

## 硬件接线

| 模块 | 引脚 | 说明 |
|---|---|---|
| OLED 0.96"（4 针 I2C） | SCL→PB8，SDA→PB9 | 3.3V 供电 |
| DHT11 | DATA→PB1 | 3.3V 供电，需 4.7k 上拉 |
| MQ-2 烟雾 | AO→PA0（ADC1_IN0） | **必须 5V 供电**；AO 需分压后再接 |
| LED | PC13 | 板载 LED |
| 蜂鸣器 | I/O→PB12 | 有源蜂鸣器（江协模块低电平响） |

> ⚠️ MQ-2 用 5V 供电时 AO 最高接近 5V，而 PA0 在模拟输入模式下**不耐 5V**，直接接会过压。
> 三种处理方式（详见 [Hardware/MQ2.h](Hardware/MQ2.h) 注释）：
> 1. 20k 串联 + 10k 对地分压，`MQ2_DIVIDER_RATIO` 填 `3.0`（推荐）
> 2. VCC 改接 3.3V（安全但灵敏度下降）
> 3. `MQ2_MODE` 改 0，DO 接 PA8（5V 容忍引脚，但只有开关量）

## 目录结构

```
Project.uvprojx      Keil5 工程文件
User/                main.c（采集→判断→显示主流程）、中断、库配置
Hardware/            OLED / DHT11 / MQ2 / Buzzer / LED / ADC / Display 驱动
System/              Delay 延时
Start/               CMSIS 内核文件 + STM32F10x 启动文件
Library/             ST 标准外设库
DHT11程序说明.md      DHT11 单总线时序与调试说明
```

主流程非常短，全部在 [User/main.c](User/main.c)：初始化 → 循环（读 DHT11 → 读 MQ-2 → 判断报警 → 输出 → 刷新 OLED）。

## 编译与烧录

1. **Keil5**（推荐）：打开 `Project.uvprojx` → Rebuild → 用 ST-Link/DAP 下载
2. **VSCode + EIDE**：导入本目录即可；`.cmsis` 器件包未入库（约 30MB），EIDE 会提示自动下载

## 常用参数在哪改

| 参数 | 位置 | 默认值 |
|---|---|---|
| 烟雾报警阈值（ADC 原始值 0~4095） | `Hardware/MQ2.h` → `MQ2_ALARM_THRESHOLD` | 1500 |
| 分压比 | `Hardware/MQ2.h` → `MQ2_DIVIDER_RATIO` | 1.0 |
| MQ-2 模式（模拟/数字） | `Hardware/MQ2.h` → `MQ2_MODE` | 1 |
| MQ-2 预热时间 | `Hardware/MQ2.h` → `MQ2_WARMUP_MS` | 20000 |
| 报警回差 | `User/main.c` → `ALARM_HYSTERESIS` | 100 |
| 蜂鸣器有效电平 | `Hardware/Buzzer.h` → `BUZZER_ON_LEVEL` | 0 |

> 阈值需要实测：预热后在洁净空气中观察 ADC 稳定值，阈值取比它明显高一些的数值。

## 效果展示

<!-- 建议放一张实物图：OLED 正常读数 + 打火机烟雾触发报警 -->
OLED 显示三行：`TEMP / HUMI`、`SMOKE` 数值、`Status: Normal / ALARM!`。

## 参考与致谢

工程结构与驱动框架参考**江协科技 STM32 入门教程**，在此基础上完成传感器接入、Display 显示模块封装、报警回差与容错处理。

## 版本记录

当前版本 **v0.10.0**（首个公开版本），完整记录见 [CHANGELOG.md](CHANGELOG.md)。

## License

[MIT](LICENSE)

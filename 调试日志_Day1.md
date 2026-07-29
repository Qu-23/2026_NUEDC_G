# 调试日志 - Day 1 (2026-07-29)

## 今日目标

搭建G题周期信号测量分析装置基础软件框架，完成采样链路和串口屏联动调试。

## 完成情况

### 1. 采样链路 ✅

- ADC1配置：PA3/INP15, TIM2触发3Msps, DMA Circular → AXI SRAM (0x2400001C)
- 缓冲区：512点, 命名段`DMA_BUF`通过scatter file放置
- D-Cache处理：每次读取前调用`SCB_InvalidateDCache_by_Addr`
- OLED副屏验证采样正常（RAW值随信号变化）

**已知问题**：
- PA2原定采样引脚硬件接线不通，已切换至PA3
- 核心板与底板拼接处信号连接需排查

### 2. 串口屏HMI框架 ✅

- USART3: PB10(TX)/PB11(RX), 115200bps
- 中断接收协议：以`\r\n`结尾的命令
- 页面状态机：0=主页, 1=波形, 2=参数, 3=频谱, 4=调试
- HMI函数：`HMI_send_string/number/float/waveform_add/spectrum_add`
- 回环测试：收到命令后原样回显1字节

**已知问题**：
- MCU TX(PB10)不工作，串口屏RX始终为0
- 需用USB-TTL模块单独验证TX通路

### 3. 关键Bug修复 ✅

| 问题 | 根因 | 修复 |
|------|------|------|
| 全速运行卡死 | semi-hosting: `printf`拉入`BKPT 0xAB` | 改用`snprintf`+`HAL_UART_Transmit` |
| L6971E链接错误 | `__attribute__((at()))`段与`.ANY`冲突 | scatter file独立执行域+命名段`DMA_BUF` |
| USART3中断风暴 | PB11浮空噪声触发中断饿死SysTick | RX引脚上拉+错误回调重开接收+TX超时保护 |

### 4. 芯片版本确认 ✅

- 读取`DBGMCU->IDCODE`，确认**非Revision V**
- ADC时钟50MHz为超频状态（非Rev-V规格≤36MHz），需在正式测试前调整

## 架构调整

### 原方案（已弃用）

```
ADC1 (12bit 高速) → 原始波形
ADC2 (16bit 低速) → Vpp + Vrms
```

### 新方案（确定）

```
AD9226 (12bit 65Msps 外部ADC) → 原始波形高速采样
ADC1 CH1 (16bit 低速过采样)   → Vpp峰值检波
ADC1 CH2 (16bit 低速过采样)   → Vrms有效值检波
```

**理由**：
- AD9226采样率远超内部ADC，原始波形质量更好
- 内部ADC1释放后可做双通道高精度低速测量
- 三路信号物理隔离，互不干扰

## 当前采样配置

| 参数 | 值 |
|------|-----|
| 芯片 | STM32H723ZGTx (非Rev-V) |
| SYSCLK | 540MHz (HSI→PLL1) |
| ADC时钟 | 50MHz (PLL2_P/2) ⚠️超频 |
| 采样引脚 | PA3 / ADC1_INP15 |
| 采样率 | 3Msps (TIM2触发) |
| 分辨率 | 12bit |
| 缓冲区 | 512点 @ AXI SRAM |
| DMA | Circular, DMA1_Stream0 |

## 待办事项

- [ ] 硬件：排查PB10(TX)焊接/走线，修复串口屏通信
- [ ] 硬件：排查核心板-底板信号连接
- [ ] 软件：ADC时钟降至36MHz（适配非Rev-V）
- [ ] 软件：AD9226外部ADC驱动开发
- [ ] 软件：ADC1双通道16bit过采样配置
- [ ] 软件：FFT频谱分析实现
- [ ] 软件：信号参数测量（Upp/Urms/基频）

# 调试日志 - Day 2 (2026-07-30)

## 今日目标

完成ADC1双通道高精度采样、串口屏双向通信、AD9226外部ADC驱动，并启动ADC2高速采样方案。

## 完成情况

### 1. ADC1 双通道高精度采样 ✅

**配置：**
- PA1 / ADC1_INP17 / Vpp (峰值检波)
- PA0 / ADC1_INP16 / Vrms (有效值检波)
- 12bit + 256x过采样 → 等效16bit (0~65520)
- 采样时间: 64.5 cycles, 扫描模式, 软件触发
- 电压换算: V = raw * 3.3 / 65536

**验证结果：** PA0和PA1均能正常响应外部信号变化

**硬件踩坑记录：**
| 引脚 | 问题 | 处理 |
|------|------|------|
| PA2 | 焊接不良,信号不通 | 换PA1 |
| PA3 | 焊接不良,信号不通 | 换PA0 |
| PA4 | 备用(可复用为ADC2高速通道) | - |

### 2. 串口屏双向通信 ✅

**硬件变更：** USART3(PB10/PB11) → USART1(PA9/PA10)
- 原因: PB10 TX始终无输出, 更换USART1后恢复正常

**TX (MCU→屏幕):**
- `HMI_send_string(name, data)`: 发送 `name.txt="data"\xff\xff\xff`
- `HMI_send_val(name, num)`: 发送 `name.val=123\xff\xff\xff`
- `HMI_curve_clear(objid, ch)`: 清空曲线通道, x归零
- `HMI_curve_add(objid, ch, val)`: 添加数据点, x自+1

**RX (屏幕→MCU):**
- 淘晶驰sendme: 页面切换时发送 `页面ID(0x00-0x04) + 0xFF 0xFF 0xFF`
- 解析: 检测连续3个0xFF作为帧结束符
- 已验证: RX计数正常, PG显示跟随页面切换

**HMI GUI页面布局 (用户更新):**
| 页面ID | 名称 | 可编辑控件 |
|--------|------|-----------|
| 0 | Home | t0.txt |
| 1 | 时域波形+参数 | Vpp.val, Vrms.val, f.val, s0曲线(512点) |
| 2 | 全屏波形 | s0曲线(1024点) |
| 3 | 电压频谱 | vb.val, v1.val, v2.val, s0曲线(600点) |
| 4 | 调试页 | t0.txt (文本) |

> 曲线图纵轴范围: 0~255

### 3. AD9226 外部ADC驱动 ✅ (基本框架)

**引脚映射：**
```
D0=PE5  D1=PC15 D2=PC14 D3=PE4  D4=PB8  D5=PB9
D6=PF12 D7=PE3  D8=PB2  D9=PB1  D10=PB0 D11=PC5
OTR=PC4  CLK=PC13
```

**驱动方式：** TIM1中断生成CLK + 多端口寄存器直接读取
- 默认采样率1MHz (TIM1: 270MHz/270)
- 可配置: AD9226_StartCapture(count, sample_rate_hz)
- 缓冲区: 4096点 @ AXI SRAM (DMA_BUF段)

**已知问题：** A值固定不变, 需排查引脚连接/时序

### 4. 架构变更: 弃用AD9226, 改用ADC2

**新方案：**
```
ADC2 (12bit 高速)    → 原始波形采样 (替代AD9226)
ADC1 CH1 (16bit低速) → Vpp峰值检波
ADC1 CH2 (16bit低速) → Vrms有效值检波
```

## 待解决: ADC2高速采样配置

### 采样率4.096MHz可行性分析

| 参数 | 4.096MHz方案 | 1MHz方案 (推荐) |
|------|-------------|----------------|
| TIM2 ARR | 270M/4.096M-1=65.9 ❌非整数 | 270M/1M-1=269 ✓ |
| 实际频率 | 4.0909MHz (误差0.12%) 或 4.0298MHz (误差1.6%) | 1MHz (精确) |
| ADC转换时间 | 14cycles@57.3MHz → 244ns | 14cycles@14MHz → 1us |
| DMA带宽 | 8.2MB/s | 2MB/s |
| 8192点FFT分辨率 | 500Hz (相干) | 122Hz (优于要求) |
| 加窗需求 | 不需要 | 需要汉宁窗 |
| 实现难度 | 高 (时钟树需精调) | 低 (现成配置) |

**结论：** 4.096MHz在STM32H723上无法精确实现 (270MHz/4.096MHz非整除)。
推荐1MHz采样率 + 8192点FFT + 汉宁窗, 分辨率122Hz远优于500Hz要求。

### ADC2引脚选择

| 引脚 | ADC2通道 | 类型 | 备注 |
|------|---------|------|------|
| PA6 | INP3 | **fast** | 推荐 |
| PA7 | INP7 | **fast** | 备选 |
| PA4 | INP18 | slow | 不推荐高速 |

### FFT方案

- CMSIS-DSP `arm_rfft_fast_f32` 支持8192点
- 需要 `arm_cfft_sR_f32_len8192` 表
- 流程: 采集8192点 → 汉宁窗 → RFFT → 取幅值 → 发送频谱曲线

## 当前系统配置总览

| 参数 | 值 |
|------|-----|
| 芯片 | STM32H723ZGTx (非Rev-V) |
| SYSCLK | 540MHz (HSI→PLL1) |
| HCLK | 270MHz |
| APB1 Timer | 270MHz (TIM2) |
| APB2 Timer | 270MHz (TIM1) |
| ADC1 | PA1(Vpp) + PA0(Vrms), 16bit过采样 |
| ADC2 | 待配置 (PA6/PA7, 12bit高速) |
| USART1 | PA9(TX)/PA10(RX), 115200bps |
| OLED | I2C/Software (4行调试显示) |

## 下一步计划

1. [ ] 配置ADC2 + DMA + TIM2触发 (PA6, 1MHz, 8192点)
2. [ ] 集成CMSIS-DSP FFT库
3. [ ] 实现波形绘制 (页面1: 512点, 页面2: 1024点)
4. [ ] 实现频谱绘制 (页面3: 600点)
5. [ ] 实现基频频率测量
6. [ ] 按键切换: 显示1/3周期波形
7. [ ] Git commit & push

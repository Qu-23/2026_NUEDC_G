# 调试日志 - Day 3 (2026-07-31)

## 今日目标

在更换新主控后，以 Day2 日志为基准逐步还原全部功能；完成 FFT 频谱分析、波形自适应显示、镜像修复；解决频率测量偏大问题；调整 ADC 时钟配置尝试突破隔触发采样瓶颈。

## 完成情况

### 1. 新主控引脚迁移与功能复原 ✅

**新主控引脚配置（参考《新MCU引脚配置.txt》）：**
| 外设 | 旧主控 | 新主控 | 备注 |
|------|--------|--------|------|
| OLED | I2C 软件模拟 | PC8=SCL / PC7=SDA | GPIO 软件模拟 IIC |
| 串口屏 | USART1(PA9/PA10) | USART3(PB10/PB11) | 通断良好 |
| ADC1 Vrms | PA0/INP16 | PA0/INP16 | 16bit 过采样 |
| ADC1 Vpp | PA1/INP17 | (暂注释) | 峰值检波精度不足,改手动计算 |
| ADC2 高速 | PA6/INP3 | PA6/INP3 | fast channel |

**复原步骤（增量验证）：**
1. 从回收站恢复 Day1 版本（commit 2ad40ac）作为基线
2. ADC1 PA0(Vrms) + PA1(Vpp) 双通道 16bit 过采样 → OLED 显示 P/R mV 值验证
3. 串口屏 RX/PG 联动恢复 → commit 004515b 推送到 dev 分支保存里程碑
4. ADC2 高速采样 + HMI 5 页面架构恢复（TIM2 TRGO 触发、DMA1_Stream1、8192 点 AXI SRAM 缓冲）

### 2. 串口屏交互稳定性修复 ✅

**Bug 现象：** 长期停留页面后 PG 失灵，需手动复位 MCU；偶发可自恢复。

**根因分析与修复：**

| Bug | 根因 | 修复 |
|-----|------|------|
| 文本控件发送无效 | 缺 `.txt` 后缀 | `HMI_send_string("t1.txt", ...)` |
| 数值控件发送无效 | 缺 `.val` 后缀 | `HMI_send_val("Vrms", ...)` |
| 曲线控件参数替换错误 | 误将 `s0.id` 替换为具体值 | 原样发送 `"s0.id"` 字符串 |
| HMI 刷新不触发 | `HAL_GetTick()` 被 SysTick(priority=15) 抢占失效 | 改用循环计数器 `hmi_div=9`（约 0.9s 周期） |
| rxState 异常卡死 | UART 错误回调未 abort 接收 | `HAL_UART_ErrorCallback` 添加 `HAL_UART_AbortReceive_IT` |
| 帧边界错位 | tx_lock 切在 PG 命令帧中间 | `HMI_tx_unlock` 中 `USART_RX_STA = 0` 重置状态机 |
| 透传超时屏幕卡死 | 0xFE 超时未发数据退出透传 | 超时分支发送 qty 字节 0x00 |

**HMI 刷新周期权衡：** 题目要求 2s 内完成测量显示，最终选定 `hmi_div=9`（约 0.9s），保证 2s 内可完成 2 次刷新且留有余量。

### 3. ADC2 高速采样与时钟诊断 ✅

**当前配置总览：**
| 参数 | 值 | 说明 |
|------|-----|------|
| ADC2 通道 | PA6 / INP3 | fast channel |
| 分辨率 | 12bit | 无过采样 |
| 采样时间 | 1.5 cycles | 最短档 |
| 触发源 | TIM2 TRGO Rising | - |
| DMA | DMA1_Stream1, Normal, 8192 点 | AXI SRAM (DMA_BUF 段) |
| PLL2 | M=32, N=80, P=2 → 80MHz | HSI=64MHz |
| ADC ClockPrescaler | `ADC_CLOCK_ASYNC_DIV1` | ADC 时钟 = 80MHz |
| BoostMode | HAL 自动配置（最高档） | H723 定义 `ADC_VER_V5_V90` |
| TIM2 | PSC=0, Period=66 | 触发频率 270MHz/67 = 4.0298MHz |
| 理论采样率 | 4.03MHz | 转换时间 14/80MHz = 175ns < 248ns 触发周期 |
| 实际采样率 | 2.015MHz（隔触发） | 见下方诊断 |

**关键发现：隔触发采样效应**
- TIM2 触发周期 = 248ns
- ADC 转换时间（理论）= 175ns < 248ns，按理不该隔触发
- 但实测 K=407 对应 100kHz 输入（FFT_FS=2.015MHz 时频率分辨率 245.97Hz × 407 = 100.1kHz）
- 即实际采样率仍为触发频率的一半，**说明隔触发未根除**

### 4. FFT 频谱分析实现 ✅

**方案选型：** 弃用 CMSIS-DSP，改用纯 C 基-2 复数 FFT（避免 `arm_rfft_fast_f32` 等符号依赖）

**核心流程：**
```
8192点 ADC2 数据
    ↓
周期汉宁窗 (hann periodic, 抑制非相干采样泄漏)
    ↓
基-2 复数 FFT (原位运算, bit-reversal)
    ↓
幅值谱 |X[k]| = sqrt(re² + im²)
    ↓
基频 bin 检索 (跳过 DC, 取最大峰)
    ↓
区间最大值降采样到 720 点 (避免漏尖峰)
    ↓
翻转数组 (修复 HMI 镜像) → curve_data
```

**关键宏（[fft.h](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Inc/fft.h)）：**
```c
#define FFT_N       8192U
#define FFT_FS      2015000.0f   /* 实际采样率(隔触发) */
#define FFT_DF      (FFT_FS / (float)FFT_N)  /* ≈245.97Hz */
```

**验证：** 100kHz 正弦输入 → K=407 → f=100.1kHz ✓

### 5. 频率测量偏大 2 倍修复 ✅

**Bug：** 初次 FFT 实现测得频率为实际 2 倍。

**根因：** FFT_FS 误用理论触发频率 4.03MHz，而实际采样率因隔触发减半为 2.015MHz。

**修复：** [fft.h](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Inc/fft.h) 中 `#define FFT_FS 2015000.0f`

### 6. 频谱显示优化 ✅

| Bug | 修复 |
|-----|------|
| 谱峰漏掉（降采样取单点） | 改为区间最大值法 `for k in [start,end): max_in_range = max(...)` |
| 频谱左右镜像 | `spectrum[spec_pts-1-i] = val` 翻转索引 |
| 谱线重叠 | 频谱页横向像素 600 → 720 |

### 7. 波形自适应显示 ✅

**1T/3T 切换：** HMI 端按键 `prints "t\r\n",0` → MCU 解析 `'t'` → `wave_mode = !wave_mode`

**自适应算法（[main.c](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/main.c)）：**
```c
float points_per_cycle = FFT_FS / fft_freq_hz;
uint16_t cycles = wave_mode ? 3 : 1;  /* 0=1T, 1=3T */
float step = points_per_cycle * cycles / PAGE1_PTS;
for (i = 0; i < PAGE1_PTS; i++) {
    float pos = i * step;
    uint32_t idx0 = (uint32_t)pos;
    float frac = pos - idx0;
    /* 线性插值 */
    float v = v0 + (v1 - v0) * frac;
    curve_data[PAGE1_PTS-1-i] = (uint8_t)((uint16_t)v >> 4);  /* 索引反转修镜像 */
}
```

**修复的 bug：**
| Bug | 修复 |
|-----|------|
| 高频波形点间压缩失真 | 基于基频自适应提取 1/3 完整周期 + 线性插值 |
| 波形左右镜像 | `curve_data[PTS-1-i]` 反转索引 |
| float 不能直接右移 | 先 `(uint16_t)v` 再 `>> 4` |
| 页面 2 全屏未适配 | case2 同步加上 1T/3T + 插值逻辑（PAGE2_PTS=1024） |

### 8. OLED 诊断显示迭代 ✅

**最终布局：**
```
L1: R:xxxx        (Vrms mV)
L2: H:xxxx L:xxxx (ADC2 max + min 0-4095)
L3: O=xxx Kyyyy   (OVR 计数 + 基频 bin 索引)
L4: PGx RXxxx     (页面/RX 计数)
```

**演进历程：**
- L3 先后显示过：`Exxx Axxx`（UART 错误/addt 超时）→ `Sxxx Pxxx txx Kxxx`（SYSCLK/PCLK1/采样时间/bin）→ `O=xxx Kyyy`（当前）
- 清理：去掉 BOOT、HMI RUN、hmi_cnt 等诊断打印

## 当前未解决问题

### 问题 1：O 值始终为 0（OVR 计数无自增）

**现象：** OLED L3 的 `O=xxx` 始终为 0，无论输入频率如何变化。

**根因：** OVR（Overrun）标志的语义是"前一次转换的数据未被读取就被新的转换覆盖"，**不是**"触发时上一次转换尚未完成"。
- 隔触发场景下，每次触发时上一次转换早已完成，DMA 已搬走数据 → 不产生 OVR
- OVR 只在 DMA 来不及搬运（带宽不足）时才会触发
- **结论：OVR 不是隔触发采样的有效诊断手段**

**建议方案：** 改用 DWT（Data Watchpoint and Trace）周期计数器精确测量采样时间：
```c
#include "core_cm7.h"
uint32_t dwt_start = DWT->CYCCNT;
ADC2_StartCapture();
while (!adc2_done);
uint32_t dwt_cycles = DWT->CYCCNT - dwt_start;
float capture_ms = dwt_cycles / 540000.0f;  /* SYSCLK=540MHz */
```
- 若 capture_ms ≈ 4.06ms（8192/2.015MHz）→ 确认仍在隔触发
- 若 capture_ms ≈ 2.03ms（8192/4.03MHz）→ 隔触发已解除，FFT_FS 需更新为 4.03MHz

### 问题 2：高频段（>150kHz）波形失真

**现象：** 120~150kHz 波形相对平滑，150kHz 以上单周期采样点数偏少，波形出现突刺（趋势轮廓不变）。

**当前 ADC 时钟实际值确认：**
- 代码中 `PLL2M=32, PLL2N=80, PLL2P=2` → PLL2P 输出 = 64/32 × 80 / 2 = **80MHz**
- `ADC_CLOCK_ASYNC_DIV1` 不分频 → **ADC 时钟 = 80MHz（非 100MHz）**
- BoostMode 由 HAL 在 `HAL_ADC_Init` 中调用 `ADC_ConfigureBoostMode` 自动配置（H723 走 `ADC_VER_V5_V90` 分支，freq/2=40MHz > 25MHz → 最高档）

**理论分析：**
- 转换时间 = 14 cycles / 80MHz = 175ns
- 触发周期 = 1/4.03MHz = 248ns
- 175ns < 248ns，**理论上不应隔触发**

**实际表现（采样率 2.015MHz 时每周期点数）：**
| 频率 | 点数/周期 | 主观质量 |
|------|----------|---------|
| 100kHz | 20.2 | 平滑 |
| 150kHz | 13.4 | 临界 |
| 200kHz | 10.1 | 开始失真 |
| 500kHz | 4.0 | 严重失真 |

用户反馈的临界点 150kHz 与 2MHz 采样率特征吻合，**说明 80MHz ADC 时钟下隔触发未根除**。

**待排查方向：**
1. PLL2 是否真正启动（HAL_RCCEx_PeriphCLKConfig 返回值检查）
2. 实际 ADC 时钟是否为 80MHz（DWT 测采样时间验证）
3. SAR 转换 cycles 是否真为 8.5（可能含额外延迟）
4. DMA 带宽是否成为瓶颈

**用户建议：** ADC 时钟改到 80MHz 或以下更稳妥。
**当前状态：** 实际已是 80MHz。若进一步降到 50MHz，转换时间升至 280ns > 248ns，反而**必然**隔触发。建议先确认 80MHz 下实际采样率，再决定是否调整。

## 当前系统配置总览

| 参数 | 值 |
|------|-----|
| 芯片 | STM32H723ZGTx (定义 `ADC_VER_V5_V90`) |
| SYSCLK | 540MHz (HSI→PLL1) |
| HCLK | 270MHz |
| APB1 Timer | 270MHz (TIM2) |
| PLL2 | M=32, N=80, P=2 → 80MHz |
| ADC2 时钟 | 80MHz (ASYNC_DIV1, BoostMode 自动最高档) |
| TIM2 触发 | 4.0298MHz (PSC=0, Period=66) |
| 实际采样率 | 2.015MHz (隔触发,待 DWT 复测) |
| ADC1 | PA0(Vrms) 16bit 过采样 |
| ADC2 | PA6 12bit 高速 + DMA |
| USART3 | PB10(TX)/PB11(RX), 115200bps |
| OLED | PC8(SCL)/PC7(SDA) 软件 IIC |
| HMI 刷新 | 0.9s 周期 (hmi_div=9) |
| FFT | 8192 点纯 C 基-2, 汉宁窗 |
| 频谱页 | 720 点曲线 |

## Git 提交记录

| Commit | 内容 |
|--------|------|
| 2ad40ac | Day1 基线恢复 |
| 004515b | ADC1 + 串口屏 RX/PG 联动里程碑 (dev 分支) |
| 0947541 | 清理诊断代码, HMI 周期 0.9s, UART ErrorCallback 修复 |
| c2bc3d8 | HMI_tx_unlock 清空状态机, 添加 E/A 诊断指标 |
| (待提交) | FFT 频谱分析, 频率测量修复, 1T/3T 自适应, 镜像修复, ADC 时钟 80MHz |

## 下一步计划

1. [ ] 用 DWT 精确测量 ADC2 采样时间，确认实际采样率（4.03MHz 还是 2.015MHz）
2. [ ] 若实际 4.03MHz：更新 `FFT_FS = 4030000.0f`，高频失真自然缓解
3. [ ] 若实际 2.015MHz：排查 PLL2 启动状态、BoostMode 寄存器实际值
4. [ ] 实现三谱线插值补偿非相干采样误差，提高频率/幅值精度
5. [ ] 手动计算峰峰值（替代 ADC1 Vpp 通道），还原放大增益
6. [ ] 频谱刻度标定（HMI 端 0~1MHz 线性刻度）
7. [ ] 优化 OLED t 值跳变（HAL_GetTick 精度问题）

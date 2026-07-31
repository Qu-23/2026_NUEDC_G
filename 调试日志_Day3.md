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

### 9. 采样率翻倍与定性功能达成 ✅

**验证现象：** ADC 时钟改至 80MHz 后，测得的基频值全部减半——虽然是错误信息（FFT_FS 未同步更新），但充分说明采样率成功翻倍，从 2.015MHz 提升到 4.03MHz。

**根因确认：** 隔触发采样已解除。
- 80MHz ADC 时钟下转换时间 175ns < 触发周期 248ns
- 每次触发时上一次转换已完成，不再跳过触发
- 实际采样率 = 触发频率 = 4.02985MHz

**修复：** [fft.h](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Inc/fft.h) 中 `FFT_FS` 从 2015000.0f 更新为 4029850.0f，频率测量恢复正常。

**定性功能达成：**
- 题目上限频点（500kHz）波形基本平滑（每周期点数翻倍至 8 点）
- 频谱图规律与理论吻合
- 1T/3T 波形自适应显示正常
- 5 页面 HMI 交互稳定

**Vpp 峰峰值计算已起步：** [main.c](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/main.c) case1 中利用 `adc2_max - adc2_min` 计算 ADC 端峰峰值（mV）并发送到 HMI 的 `Vpp.val` 控件。增益还原待后续补充。

**下一步转入定量指标实现。**

### 10. 定量指标 P0 完成 ✅

**P0-1 真有效值软件计算：**
- [main.c](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/main.c#L187-L196) 缓冲区分析中实现
- `Urms = sqrt((1/N)*Σ(xi-mean)²)`，用 uint64 累加避免溢出
- 减去直流分量 `adc2_avg` 再平方，去除 DC 偏置
- 适用于任意波形（基波+0~2 个谐波均准确）
- **验证结果：** 对比示波器周期有效值，多数点位误差 < 1mV，精度极高

**P0-2 Vpp 增益还原：**
- 前级总增益 5.78 倍（线性性已验证，含模拟低通衰减）
- `FRONT_GAIN` 宏定义移至 [fft.h](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Inc/fft.h) 统一管理
- Vpp/Vrms 均还原为信号源端电压（mV）
- **验证结果：** HMI 显示 Vpp 与示波器 math 模式精确理论值所差无几

**P0-3 OLED 显示改造：**
- 删除无用调试信息（H/L/OVR 等）
- 改为软件/硬件 Urms 对比显示，方便精度对比
```
L1: S:xxxx H:xxxx  (软件Urms / 硬件Urms, 信号源端mV)
L2: P:xxxx         (Vpp mV, 已还原增益)
L3: f:xxxx K:xxxx  (基频kHz / bin索引)
L4: PGx RXxxx      (页面/RX计数)
```
- **结论：** 软件 Urms 精度更高，但硬件检波可用作备用。暂保留软件计算

### 11. 频谱绝对高度校准 ✅

**问题：** 原归一化方式以全局 max_mag 为满量程（相对高度），无法体现实际幅值的绝对大小。当谐波幅值与基频相同时，两根谱线高度接近，但无法直接读出幅值。

**改进：** 改为绝对高度——FFT 幅值转换为信号源端 mV，直接对应曲线像素高度。
- [fft.c](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/fft.c#L138-L143) 降采样归一化改为：
  ```c
  float amp_mv = FFT_MAG_TO_MV(max_in_range);
  uint32_t val = (uint32_t)amp_mv;
  if (val > 250) val = 250;
  if (val < 20) val = 0;  /* 噪声截断 */
  ```
- [fft.h](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Inc/fft.h) 新增统一宏：
  ```c
  #define FFT_MAG_TO_MV(mag)  ((mag) * 2.0f / FFT_N * 2.0f * 3.3f / 4096.0f * 1000.0f / FRONT_GAIN)
  ```
- 对应关系：50mV 信号 → 50 像素高度，250mV 信号 → 250 像素高度（顶格）
- `FFT_GetAmplitude` 也改用统一宏，保证幅值计算一致性
- **效果：** 谱线高度直接反映实际幅值，符合题目定性显示要求

### 12. 分量幅值校准与谱线Upp映射 ✅

**问题定位：** 频谱绝对高度绘制已与实际输出吻合，但 HMI 上 `vb/v1/v2` 数值与谱线高度对不上——经核查根因是单位缩放差 10 倍：
- `FFT_GetAmplitude` 返回 V → `HMI_send_float` 又 `×100` 转"厘伏"
- 50mV(V_peak) 信号 → `vb.val=5`，但谱线高度 50 像素

**关键认知：** 题目要求的"分量范围 50~250mV"指 **Upp（峰峰值）**，而 FFT 校准得到的 `FFT_MAG_TO_MV` 是 **V_peak（幅值）**。二者是 2 倍关系。

**最小修改（分两步）：**

| 步骤 | 修改 | 文件 |
|------|------|------|
| ① vb/v1/v2 改为发 mV 整数 | `HMI_send_val("vb", (uint16_t)(FFT_GetAmplitude(k,1)*1000))` | [main.c case3](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/main.c#L290-L306) |
| ② 谱线像素×2 映射 Upp | `FFT_MAG_TO_MV(max_in_range) * 2.0f` | [fft.c L141](file:///c:/Users/kafel/Desktop/PROJ_EX/2026_NUEDC/TST_H723/Core/Src/fft.c#L141) |

**最终物理口径：**
- `vb/v1/v2`：V_peak（mV）—— 定量指标要求"测幅值"，保持不变
- 谱线像素：Upp（mV）→ 像素，50mV Upp→50px，250mV Upp→250px

**噪声阈值调整：** `V_peak < 15mV`（即 Upp<30mV）截断。原阈值 V_peak<20mV 偏狠，会把小信号分量削没；改为 15mV 在信号源下限 50mV Upp 之下留余量，又能压制频域细小噪声。

**验证结果：** 各分量幅值与谱线高度基本吻合，且 `vb.val ≈ Upp/2`，定量指标基本实现。

### 13. 第3项抗干扰硬件确认 ✅

**关键信息：** 前级电路中已有模拟低通滤波器，5.78 的系统增益包含滤波电路的衰减在内。
- **结论：** 当前测量已是"连带要求3一同实现"的状态
- uJ（200mV, fJ≥1MHz）由硬件低通滤除，无需软件数字滤波
- 后续只需验证加 1MHz 干扰时测量精度是否仍达标

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
| 实际采样率 | 4.02985MHz (隔触发已解除) |
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
| 6239286 | OVR 诊断+波形镜像修复+频谱 720 点 |
| dcd6493 | 采样率翻倍: ADC 时钟 80MHz 解除隔触发, FFT_FS=4.03MHz, Vpp 计算 |
| 537a751 | P0: 真有效值软件计算+Vpp/Vrms 增益还原(5.78 倍) |
| 8029b7d | OLED 改 Urms 对比+频谱归一化 20~250 噪声截断 |
| 3ee1d21 | 频谱绝对高度: FFT 幅值转 mV 对应像素, FRONT_GAIN 移至 fft.h |
| (本次) | 分量幅值mV整数发送+谱线×2映射Upp+阈值V_peak<15mV |

## 下一步计划

### 已完成（锁定）
1. [x] 采样率翻倍确认: ADC 时钟 80MHz 解除隔触发, FFT_FS=4.02985MHz
2. [x] Vpp 峰峰值计算 + 增益还原(5.78 倍), 验证通过
3. [x] 真有效值软件计算, 精度 < 1mV, 验证通过
4. [x] 频谱绝对高度校准(50mV Upp→50px, 250mV Upp→250px)
5. [x] 第3项抗干扰硬件确认(前级模拟低通已含在 5.78 增益内)
6. [x] P0-3: FFT 分量幅值校准(vb/v1/v2 发送 V_peak mV, 与谱线高度口径一致)

### 待实现（按优先级排序）

**P0 - 精度提升（直接影响定量指标误差）**
7. [ ] 三谱线插值补偿非相干采样误差
   - 现状: ±1 bin 取最大值, 非相干采样下能量仍泄漏到旁瓣, 幅值偏小
   - 方案: 抛物线插值 / Quinn's estimator, 误差可降至 <1%
   - 同时提升频率精度(目前 K×FFT_DF 量化误差 ±245Hz)

8. [ ] 多组数据全量验证(频率×幅值矩阵)
   - 题目要求频率 10kHz~500kHz × 幅值 50~250mV Upp
   - 重点验证: 10kHz(低频端 K 值小) + 500kHz(高频端点数少) + 50mV(小信号噪声) + 250mV(大信号饱和)
   - 记录各点 f/Vpp/Vrms/vb/v1/v2 误差, 量化系统精度边界

**P1 - 题目完成度补齐**
9. [ ] 第3项验证: 加 1MHz 干扰(uJ, 200mV)时测量精度是否仍达标
   - 硬件低通已确认, 但需实测验证软件读数不漂移
   - 关注: 1MHz 在 FFT 上是否折叠到基带产生虚假谱线

10. [ ] 频谱刻度标定 (HMI 端 0~2MHz 线性刻度)
    - 当前频谱 720 点横跨 0~2.015MHz, 每像素 ≈2.8kHz
    - HMI 端绘制频率刻度线 + 数值标签(100k/200k/.../2M)

**P2 - 体验优化（非必需）**
11. [ ] 高频段(>150kHz)波形失真优化
    - 当前 4.03MHz 采样率下 500kHz 仅 8 点/周期
    - 方向: 软件正弦拟合还原 / 时域插值上采样
12. [ ] OLED/HMI 信息排版微调(基于全量验证结果)

## 优化方向总结

**核心矛盾:** 当前定性 + P0 定量基本达成, 但精度仍有提升空间。下一步主线是**三谱线插值**(P0-7)——一举提升频率 + 幅值精度, 是性价比最高的优化点。随后用**全量矩阵验证**(P0-8)摸清系统精度边界, 再针对性补齐高频端 / 小信号端 / 1MHz 抗干扰等薄弱环节。

**风险点:** 高频波形失真(>150kHz)若题目要求时域波形保真, 可能需要软件拟合; 否则可优先保频域精度即可。

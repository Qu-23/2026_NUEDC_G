#ifndef __FFT_H__
#define __FFT_H__

#include <stdint.h>

/* FFT点数, 与ADC2_BUF_SIZE一致 (8192 = 2^13) */
#define FFT_N       8192U
#define FFT_N_HALF  (FFT_N / 2U)

/* 采样率: TIM2触发270MHz/67=4.02985MHz, ADC时钟80MHz下转换时间175ns<触发周期248ns,
   隔触发已解除, 实际采样率 = 触发频率 = 4.02985MHz */
#define FFT_FS      4029850.0f

/* 频率分辨率: fs/N = 491.67Hz */
#define FFT_DF      (FFT_FS / (float)FFT_N)

/* 前级放大器总增益(线性性已验证, 含模拟低通衰减) */
#define FRONT_GAIN  5.78f

/* ADC参考电压与满量程 */
#define ADC_VREF    3.3f
#define ADC_FULL    4096.0f

/* FFT幅值转mV: mag * 2/N * 2(汉宁窗补偿) * VREF/FULL * 1000 / FRONT_GAIN */
#define FFT_MAG_TO_MV(mag)  ((mag) * 2.0f / (float)FFT_N * 2.0f * ADC_VREF / ADC_FULL * 1000.0f / FRONT_GAIN)

/* 复数结构体 */
typedef struct {
    float re;
    float im;
} cplx_float_t;

/**
 * @brief  FFT处理: 从ADC2原始数据计算频谱
 * @param  adc_data: ADC2采样数据 (uint16_t, 8192点)
 * @param  spectrum: 输出频谱幅值 (uint8_t, 600点, 0~255)
 * @param  spec_pts: 频谱曲线点数 (通常600)
 * @retval 基频bin索引 (0表示未找到)
 */
uint32_t FFT_Process(const uint16_t *adc_data, uint8_t *spectrum, uint16_t spec_pts);

/**
 * @brief  从FFT结果计算基频频率
 * @param  k_peak: 基频bin索引
 * @retval 频率值 (Hz)
 */
float FFT_GetFrequency(uint32_t k_peak);

/**
 * @brief  从FFT结果计算指定谐波次数的幅值
 * @param  k_fundamental: 基频bin索引
 * @param  harmonic: 谐波次数 (1=基波, 2=二次谐波, 3=三次谐波)
 * @retval 幅值 (V)
 */
float FFT_GetAmplitude(uint32_t k_fundamental, uint8_t harmonic);

#endif /* __FFT_H__ */

#ifndef __FFT_H__
#define __FFT_H__

#include <stdint.h>

/* FFT点数, 与ADC2_BUF_SIZE一致 (8192 = 2^13) */
#define FFT_N       8192U
#define FFT_N_HALF  (FFT_N / 2U)

/* 采样率: TIM2触发 135MHz/40 = 3.375MHz */
#define FFT_FS      3375000.0f

/* 频率分辨率: fs/N = 412Hz (满足赛题500Hz步进要求) */
#define FFT_DF      (FFT_FS / (float)FFT_N)

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

#include "fft.h"
#include "adc.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ===== 全局变量 (不放栈, 避免溢出) ===== */
/* fft_buf: 64KB, win_buf: 32KB, fft_mag: 16KB, 总计112KB */
static cplx_float_t fft_buf[FFT_N];
static float win_buf[FFT_N];
static float fft_mag[FFT_N_HALF];
static uint8_t win_inited = 0;

/* ===== 周期汉宁窗 hann(N,'periodic') ===== */
static void hann_periodic(float *win, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        win[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * (float)i / (float)n));
}

/* ===== 基-2复数FFT (原位运算, bit-reversal) ===== */
static void fft_cplx(cplx_float_t *x, uint32_t n)
{
    uint32_t i, j, k, m, mh;
    float wr, wi, tr, ti;

    /* bit reversal置换 */
    j = 0;
    for (i = 1; i < n; i++)
    {
        uint32_t bit = n >> 1;
        for (; j >= bit; bit >>= 1)
            j -= bit;
        j += bit;
        if (i < j)
        {
            cplx_float_t tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
    }

    /* 蝶形运算 */
    for (m = 2; m <= n; m <<= 1)
    {
        mh = m >> 1;
        float ang = -2.0f * M_PI / (float)m;
        wr = cosf(ang);
        wi = sinf(ang);

        for (i = 0; i < n; i += m)
        {
            cplx_float_t w = {1.0f, 0.0f};
            for (j = 0; j < mh; j++)
            {
                uint32_t p = i + j;
                uint32_t q = p + mh;
                tr = w.re * x[q].re - w.im * x[q].im;
                ti = w.re * x[q].im + w.im * x[q].re;

                x[q].re = x[p].re - tr;
                x[q].im = x[p].im - ti;
                x[p].re = x[p].re + tr;
                x[p].im = x[p].im + ti;

                float w_re_new = w.re * wr - w.im * wi;
                float w_im_new = w.re * wi + w.im * wr;
                w.re = w_re_new;
                w.im = w_im_new;
            }
        }
    }
}

/* ===== FFT主处理: ADC数据 → 频谱曲线 + 基频bin ===== */
uint32_t FFT_Process(const uint16_t *adc_data, uint8_t *spectrum, uint16_t spec_pts)
{
    /* 1. 预计算汉宁窗 (仅首次) */
    if (!win_inited)
    {
        hann_periodic(win_buf, FFT_N);
        win_inited = 1;
    }

    /* 2. 计算直流分量 (去直流, 否则bin0过大影响显示) */
    uint32_t dc_sum = 0;
    for (uint32_t i = 0; i < FFT_N; i++)
        dc_sum += adc_data[i];
    float dc = (float)dc_sum / (float)FFT_N;

    /* 3. ADC数据 → 减直流 → 乘汉宁窗 → 放入FFT缓冲区 */
    for (uint32_t i = 0; i < FFT_N; i++)
    {
        float sample = (float)adc_data[i] - dc;
        fft_buf[i].re = sample * win_buf[i];
        fft_buf[i].im = 0.0f;
    }

    /* 4. 执行FFT */
    fft_cplx(fft_buf, FFT_N);

    /* 5. 计算前N/2点幅值 */
    float max_mag = 0.0f;
    uint32_t max_idx = 0;
    for (uint32_t k = 0; k < FFT_N_HALF; k++)
    {
        float mag = sqrtf(fft_buf[k].re * fft_buf[k].re + fft_buf[k].im * fft_buf[k].im);
        fft_mag[k] = mag;
        /* 跳过bin0(直流), 从bin1开始找基频 */
        if (k > 0 && mag > max_mag)
        {
            max_mag = mag;
            max_idx = k;
        }
    }

    /* 6. 降采样到spec_pts点, 归一化到0~255 */
    if (spectrum != NULL && spec_pts > 0)
    {
        for (uint16_t i = 0; i < spec_pts; i++)
        {
            uint32_t idx = (uint32_t)i * FFT_N_HALF / spec_pts;
            if (idx >= FFT_N_HALF)
                idx = FFT_N_HALF - 1;
            /* 归一化: 以max_mag为满量程255 */
            uint32_t val = (uint32_t)(fft_mag[idx] * 255.0f / max_mag);
            if (val > 255)
                val = 255;
            spectrum[i] = (uint8_t)val;
        }
    }

    return max_idx;
}

/* ===== 基频频率计算 ===== */
float FFT_GetFrequency(uint32_t k_peak)
{
    return (float)k_peak * FFT_DF;
}

/* ===== 谐波幅值计算 (V) ===== */
/* 幅值 = mag * 2/N * 窗补偿(2.0) * 电压缩放(3.3/4096) */
float FFT_GetAmplitude(uint32_t k_fundamental, uint8_t harmonic)
{
    uint32_t k = k_fundamental * harmonic;
    if (k >= FFT_N_HALF)
        return 0.0f;

    /* 取峰值附近最大值 (±1 bin) */
    float mag = fft_mag[k];
    if (k > 0 && fft_mag[k - 1] > mag)
        mag = fft_mag[k - 1];
    if (k + 1 < FFT_N_HALF && fft_mag[k + 1] > mag)
        mag = fft_mag[k + 1];

    /* 幅值校正: *2/N(实信号FFT) * 2.0(汉宁窗能量补偿) * 3.3/4096(ADC电压) */
    return mag * 2.0f / (float)FFT_N * 2.0f * 3.3f / 4096.0f;
}

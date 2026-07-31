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

/* ===== 基-2复数IFFT (复用fft_cplx: IFFT = conj(FFT(conj(x)))/N) ===== */
static void ifft_cplx(cplx_float_t *x, uint32_t n)
{
    /* 1. 取共轭 */
    for (uint32_t i = 0; i < n; i++)
        x[i].im = -x[i].im;
    /* 2. FFT */
    fft_cplx(x, n);
    /* 3. 取共轭并除以N */
    float inv_n = 1.0f / (float)n;
    for (uint32_t i = 0; i < n; i++)
    {
        x[i].re *= inv_n;
        x[i].im = -x[i].im * inv_n;
    }
}

/* ===== FFT主处理: ADC数据 → 频谱曲线 + 基频bin ===== */
uint32_t FFT_Process(const uint16_t *adc_data, uint8_t *spectrum, uint16_t spec_pts, uint16_t *filtered)
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

    /* 4.5 软件滤波: 软过渡衰减 0.9MHz~1.1MHz 分量 (Gibbs振铃缓解)
       实信号FFT共轭对称: fft_buf[N-k] = conj(fft_buf[k]), 需同时处理 */
    {
        uint32_t k_cut = (uint32_t)(900000.0f / FFT_DF);   /* 0.9MHz 开始衰减 */
        uint32_t k_stop = (uint32_t)(1100000.0f / FFT_DF); /* 1.1MHz 完全截止 */
        if (k_stop > FFT_N_HALF) k_stop = FFT_N_HALF;
        if (k_cut >= k_stop) k_cut = k_stop > 0 ? k_stop - 1 : 0;
        for (uint32_t k = k_cut; k < FFT_N_HALF; k++)
        {
            float gain;
            if (k < k_stop)
                gain = 1.0f - (float)(k - k_cut) / (float)(k_stop - k_cut);
            else
                gain = 0.0f;
            fft_buf[k].re *= gain;
            fft_buf[k].im *= gain;
            /* 共轭对称: N-k 分量 */
            uint32_t k_sym = FFT_N - k;
            if (k_sym != k)
            {
                fft_buf[k_sym].re *= gain;
                fft_buf[k_sym].im *= gain;
            }
        }
    }

    /* 5. 计算前N/2点幅值 (用滤波后的fft_buf) */
    for (uint32_t k = 0; k < FFT_N_HALF; k++)
    {
        fft_mag[k] = sqrtf(fft_buf[k].re * fft_buf[k].re + fft_buf[k].im * fft_buf[k].im);
    }

    /* 6. 基频查找: 组合信号中频率最低的谱峰 (老师指导意见)
       原逻辑: 找全局最大幅值峰 → 谐波幅值大于基频时会误判
       新逻辑: 找第一个超过阈值的局部极大值(即频率最低的有效谱峰)
       算法: 从bin2开始(跳过DC附近), 找 fft_mag[k]>阈值 且为局部极大值的第一个k */
    uint32_t max_idx = 0;
    for (uint32_t k = 2; k < FFT_N_HALF - 2; k++)
    {
        if (fft_mag[k] > FFT_MAG_THRESH &&
            fft_mag[k] > fft_mag[k - 1] &&
            fft_mag[k] > fft_mag[k + 1])
        {
            max_idx = k;
            break;  /* 找到第一个(频率最低)即停 */
        }
    }

    /* 7. 降采样到spec_pts点, 归一化到0~250+噪声截断 (取区间最大值, 避免漏掉尖峰) */
    if (spectrum != NULL && spec_pts > 0)
    {
        uint32_t bin_per_pt = FFT_N_HALF / spec_pts;
        if (bin_per_pt == 0) bin_per_pt = 1;
        for (uint16_t i = 0; i < spec_pts; i++)
        {
            uint32_t start = (uint32_t)i * bin_per_pt;
            uint32_t end = start + bin_per_pt;
            if (end > FFT_N_HALF) end = FFT_N_HALF;

            /* 找区间内最大值 */
            float max_in_range = 0.0f;
            for (uint32_t k = start; k < end; k++)
            {
                if (fft_mag[k] > max_in_range)
                    max_in_range = fft_mag[k];
            }

            /* 绝对高度: FFT幅值转mV×2=Upp对应像素 (50mV Upp→50像素, 250mV Upp→250像素)
               vb/v1/v2仍发V_peak(定量指标测幅值), 谱线按Upp画便于直接读源端峰峰值
               还原前级增益, 噪声截断(V_peak<15mV置0), 翻转数组(HMI镜像) */
            float amp_mv = FFT_MAG_TO_MV(max_in_range) * 2.0f;
            uint32_t val = (uint32_t)amp_mv;
            if (val > 250) val = 250;
            if (val < 30) val = 0;  /* V_peak<15mV(即Upp<30mV)截断, 避免削掉小信号分量 */
            spectrum[spec_pts - 1 - i] = (uint8_t)val;
        }
    }

    /* 8. IFFT回时域, 输出滤波后波形 (用于Vpp/Urms/时域显示)
       IFFT后fft_buf[i].re是 (x[n]-dc)*w[n] 的滤波后版本
       加回dc: 中间区域(w≈1)≈x[n]滤波后值; 两端(w≈0)趋近dc电平 */
    if (filtered != NULL)
    {
        ifft_cplx(fft_buf, FFT_N);
        for (uint32_t i = 0; i < FFT_N; i++)
        {
            float val = fft_buf[i].re + dc;
            if (val < 0.0f) val = 0.0f;
            if (val > 4095.0f) val = 4095.0f;
            filtered[i] = (uint16_t)val;
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
    /* 使用统一宏 FFT_MAG_TO_MV 转mV, 再除1000转V */
    return FFT_MAG_TO_MV(mag) / 1000.0f;
}

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file is available, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED_EX.h"
#include "fft.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* FRONT_GAIN 已移至 fft.h, 供 fft.c 与 main.c 共用 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 页面: 0=主页, 1=波形, 2=参数, 3=频谱, 4=调试 */
int8_t current_page = 0;
uint8_t rx_cnt = 0;
uint8_t wave_mode = 0;  /* 0=1T(默认), 1=3T, HMI发送't'切换 */

/* HMI曲线交互参数 */
#define PAGE1_PTS      512      /* 页面1横向像素 */
#define PAGE2_PTS      1024     /* 页面2横向像素 */
#define PAGE3_PTS      720      /* 页面3横向像素(频谱) */

/* addt透传数据缓冲(最大PAGE2_PTS=1024点) */
static uint8_t curve_data[PAGE2_PTS];

/* FFT诊断变量 (OLED第三行显示) */
uint32_t fft_k_peak = 0;    /* 基频bin索引 */
float fft_freq_hz = 0.0f;   /* 基频频率(Hz) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Initialize(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC2_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  /* 串口屏就绪, 等待主循环刷新 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 解析串口屏页面切换命令 */
    if (USART_RX_STA & 0x8000)
    {
      uint8_t cmd = USART_RX_BUF[0];
      USART_RX_STA = 0;
      rx_cnt++;
      switch (cmd)
      {
        case '0': current_page = 0; break;
        case '1': current_page = 1; break;
        case '2': current_page = 2; break;
        case '3': current_page = 3; break;
        case '4': current_page = 4; break;
        case 't': wave_mode = !wave_mode; break;  /* 1T/3T切换 */
      }
    }

    /* ADC2周期采样 (OLED验证 + HMI波形数据源, 每200ms一次) */
    {
      static uint32_t last_adc2 = 0;
      if (adc2_done && HAL_GetTick() - last_adc2 >= 200)
      {
        last_adc2 = HAL_GetTick();
        ADC2_StartCapture();
      }
    }

    /* ADC2缓冲区分析 (OLED验证: max/min/avg + 真有效值 + 触发同步) */
    static uint16_t adc2_max = 0, adc2_min = 4095, adc2_avg = 0;
    static float urms_mv = 0;  /* 软件真有效值(信号源端mV, 已还原增益) */
    static uint8_t adc2_analyzed = 0;
    static uint32_t trig_pos = 0;  /* 上升过零触发点(波形示波器风格稳定显示) */
    if (adc2_done && !adc2_analyzed)
    {
      uint32_t sum = 0;
      uint16_t mx = 0, mn = 4095;
      for (uint16_t i = 0; i < ADC2_BUF_SIZE; i++)
      {
        uint16_t v = adc2_buf[i];
        if (v > mx) mx = v;
        if (v < mn) mn = v;
        sum += v;
      }
      adc2_max = mx;
      adc2_min = mn;
      adc2_avg = sum / ADC2_BUF_SIZE;
      /* 真有效值: Urms = sqrt((1/N)*Σ(xi-mean)²), 还原前级增益 */
      /* 用uint64累加避免溢出(4095²×8192≈1.37e11 > uint32上限) */
      uint64_t sum_sq = 0;
      for (uint16_t i = 0; i < ADC2_BUF_SIZE; i++)
      {
        int32_t diff = (int32_t)adc2_buf[i] - (int32_t)adc2_avg;
        sum_sq += (uint64_t)(diff * diff);
      }
      float rms_raw = sqrtf((float)sum_sq / ADC2_BUF_SIZE);
      urms_mv = rms_raw * 3300.0f / 4096.0f / FRONT_GAIN;

      /* 上升过零触发点查找: 减去DC后, 在缓冲区中段(避开边界噪声)找第一个 s[i]<=0 && s[i+1]>0
         用于波形示波器风格稳定显示, 避免每次刷新起点相位随机导致波形左右乱晃 */
      int32_t mid = (int32_t)adc2_avg;
      trig_pos = 0;
      for (uint32_t i = ADC2_BUF_SIZE / 4; i < ADC2_BUF_SIZE * 3 / 4 - 1; i++)
      {
        int32_t s0 = (int32_t)adc2_buf[i] - mid;
        int32_t s1 = (int32_t)adc2_buf[i + 1] - mid;
        if (s0 <= 0 && s1 > 0) { trig_pos = i; break; }
      }
      adc2_analyzed = 1;
    }
    else if (!adc2_done)
    {
      adc2_analyzed = 0;
    }

    /* HMI数据刷新 (循环计数器, 约0.4秒周期, 题目要求2s内留余量) */
    {
      static uint8_t hmi_div = 0;
      if (++hmi_div >= 4)  /* 约0.4秒(4次循环×~100ms), 2s内可完成5次刷新, 视觉接近连续 */
      {
        hmi_div = 0;

        /* FFT预处理: case1需要频率, case3需要频谱+谐波幅值 */
        if (current_page == 1 || current_page == 3)
        {
          uint32_t t0 = HAL_GetTick();
          while (!adc2_done && HAL_GetTick() - t0 < 10) {}
          /* FFT: 8192点 → 频谱600点存入curve_data + 基频bin索引 */
          fft_k_peak = FFT_Process(adc2_buf, curve_data, PAGE3_PTS);
          fft_freq_hz = FFT_GetFrequency(fft_k_peak);
        }

        switch (current_page)
        {
          case 1: /* 时域波形+参数, 自适应1T/3T周期显示 */
          {
            /* 根据基频从adc2_buf提取1或3个完整周期, 线性插值到512点 */
            if (fft_freq_hz > 100.0f)
            {
              float points_per_cycle = FFT_FS / fft_freq_hz;
              uint16_t cycles = wave_mode ? 3 : 1;  /* 0=1T, 1=3T */
              float step = points_per_cycle * cycles / PAGE1_PTS;
              for (uint16_t i = 0; i < PAGE1_PTS; i++)
              {
                float pos = i * step;
                uint32_t idx0 = (uint32_t)pos;
                float frac = pos - idx0;
                /* 起点偏移trig_pos: 上升过零触发同步, 波形稳定显示 */
                uint16_t v0 = adc2_buf[(trig_pos + idx0) % ADC2_BUF_SIZE];
                uint16_t v1 = adc2_buf[(trig_pos + idx0 + 1) % ADC2_BUF_SIZE];
                float v = v0 + (v1 - v0) * frac;
                curve_data[PAGE1_PTS-1-i] = (uint8_t)((uint16_t)v >> 4);  /* 反转索引修复HMI镜像 */
              }
            }
            else
            {
              /* 频率无效时降级: 直接降采样 */
              for (uint16_t i = 0; i < PAGE1_PTS; i++)
                curve_data[PAGE1_PTS-1-i] = adc2_buf[i * 16] >> 4;
            }
            HMI_tx_lock();
            HMI_curve_clear("s0.id", 0);
            HMI_curve_addt("s0.id", 0, curve_data, PAGE1_PTS);
            HMI_send_val("Vrms", (int)urms_mv);  /* 软件真有效值(信号源端mV) */
            HMI_send_val("Vpp", (int)((float)(adc2_max - adc2_min) * 3300.0f / 4096.0f / FRONT_GAIN));  /* 信号源端峰峰值mV */
            HMI_send_val("f", (int)(fft_freq_hz / 1000.0f));  /* kHz */
            HMI_tx_unlock();
            break;
          }
          case 2: /* 全屏波形, 自适应1T/3T周期显示 */
          {
            uint32_t t0 = HAL_GetTick();
            while (!adc2_done && HAL_GetTick() - t0 < 10) {}
            /* 根据基频从adc2_buf提取1或3个完整周期, 线性插值到1024点 */
            if (fft_freq_hz > 100.0f)
            {
              float points_per_cycle = FFT_FS / fft_freq_hz;
              uint16_t cycles = wave_mode ? 3 : 1;  /* 0=1T, 1=3T */
              float step = points_per_cycle * cycles / PAGE2_PTS;
              for (uint16_t i = 0; i < PAGE2_PTS; i++)
              {
                float pos = i * step;
                uint32_t idx0 = (uint32_t)pos;
                float frac = pos - idx0;
                /* 起点偏移trig_pos: 上升过零触发同步, 波形稳定显示 */
                uint16_t v0 = adc2_buf[(trig_pos + idx0) % ADC2_BUF_SIZE];
                uint16_t v1 = adc2_buf[(trig_pos + idx0 + 1) % ADC2_BUF_SIZE];
                float v = v0 + (v1 - v0) * frac;
                curve_data[PAGE2_PTS-1-i] = (uint8_t)((uint16_t)v >> 4);  /* 反转索引修复HMI镜像 */
              }
            }
            else
            {
              /* 频率无效时降级: 直接降采样 */
              for (uint16_t i = 0; i < PAGE2_PTS; i++)
                curve_data[PAGE2_PTS-1-i] = adc2_buf[i * 8] >> 4;
            }
            HMI_tx_lock();
            HMI_curve_clear("s0.id", 0);
            HMI_curve_addt("s0.id", 0, curve_data, PAGE2_PTS);
            HMI_tx_unlock();
            break;
          }
          case 3: /* 电压频谱 */
          {
            /* curve_data已被FFT_Process填充为频谱数据(600点) */
            /* 调试: 直接发送谱线绝对高度对应的幅值(mV), 与谱线像素高度数值一致
               FFT_GetAmplitude返回V, ×1000还原为mV (与FFT_MAG_TO_MV同一口径) */
            uint16_t vb_mv = (uint16_t)(FFT_GetAmplitude(fft_k_peak, 1) * 1000.0f);
            uint16_t v1_mv = (uint16_t)(FFT_GetAmplitude(fft_k_peak, 2) * 1000.0f);
            uint16_t v2_mv = (uint16_t)(FFT_GetAmplitude(fft_k_peak, 3) * 1000.0f);
            HMI_tx_lock();
            HMI_curve_clear("s0.id", 0);
            HMI_curve_addt("s0.id", 0, curve_data, PAGE3_PTS);
            HMI_send_val("vb", vb_mv);
            HMI_send_val("v1", v1_mv);
            HMI_send_val("v2", v2_mv);
            HMI_tx_unlock();
            break;
          }
          case 4: /* 调试页 (t1-t8可用) */
          {
            char dbg[32];
            snprintf(dbg, sizeof(dbg), "P%d R%d", current_page, rx_cnt);
            HMI_tx_lock();
            HMI_send_string("t1.txt", dbg);
            HMI_tx_unlock();
            break;
          }
          default: /* Home页无控件, 不发送 */
            break;
        }
      }
    }

    /* OLED显示: 软件Urms + Vpp + 基频 (ADC1已移除, L1右半暂留空待填补偿规律)
       L1: S:xxxx         (软件Urms mV, 信号源端)
       L2: P:xxxx         (Vpp mV, 已还原增益)
       L3: f:xxxx K:xxxx  (基频kHz / bin索引)
       L4: PGx RXxxx      (页面/RX计数)
    */
    uint16_t vpp_mv = (uint16_t)((float)(adc2_max - adc2_min) * 3300.0f / 4096.0f / FRONT_GAIN);

    OLED_ShowString(1,1,"S:");
    OLED_ShowNum(1,3,(uint16_t)urms_mv,4);

    OLED_ShowString(2,1,"P:");
    OLED_ShowNum(2,3,vpp_mv,4);

    OLED_ShowString(3,1,"f:");
    OLED_ShowNum(3,3,(uint16_t)(fft_freq_hz/1000.0f),4);
    OLED_ShowString(3,8,"K:");
    OLED_ShowNum(3,10,fft_k_peak,4);

    OLED_ShowString(4,1,"PG");
    OLED_ShowNum(4,3,current_page,1);
    OLED_ShowString(4,5,"RX");
    OLED_ShowNum(4,7,rx_cnt,3);

    HAL_Delay(100);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 135;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

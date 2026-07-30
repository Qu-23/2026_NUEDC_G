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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 页面: 0=主页, 1=波形, 2=参数, 3=频谱, 4=调试 */
int8_t current_page = 0;
uint8_t rx_cnt = 0;

/* HMI曲线交互参数 */
#define CURVE_OBJ_ID   1        /* s0曲线控件的对象ID */
#define PAGE1_PTS      512      /* 页面1横向像素 */
#define PAGE2_PTS      1024     /* 页面2横向像素 */
#define HMI_PERIOD_MS  1500     /* HMI刷新周期(题目要求2s内, 留500ms余量) */

/* addt透传数据缓冲(最大PAGE2_PTS=1024点) */
static uint8_t curve_data[PAGE2_PTS];
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
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  /* 串口屏通信测试 */
  HMI_send_string("t5.txt", "MCU OK");
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
      }
    }

    /* ADC1双通道采样 (256x过采样, ≈0.8ms/次) */
    ADC_Read_Channels();

    /* ADC2周期采样 (OLED验证 + HMI波形数据源, 每200ms一次) */
    {
      static uint32_t last_adc2 = 0;
      if (adc2_done && HAL_GetTick() - last_adc2 >= 200)
      {
        last_adc2 = HAL_GetTick();
        ADC2_StartCapture();
      }
    }

    /* ADC2缓冲区分析 (OLED验证: done/max/min/avg) */
    static uint16_t adc2_max = 0, adc2_min = 4095, adc2_avg = 0;
    static uint8_t adc2_analyzed = 0;
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
      adc2_analyzed = 1;
    }
    else if (!adc2_done)
    {
      adc2_analyzed = 0;
    }

    /* HMI数据刷新 (周期性, 题目要求2s内完成处理和显示) */
    {
      static uint32_t last_hmi = 0;
      if (HAL_GetTick() - last_hmi >= HMI_PERIOD_MS)
      {
        last_hmi = HAL_GetTick();
        switch (current_page)
        {
          case 1: /* 时域波形+参数 */
          {
            /* 等待最新周期采样完成 (最多等10ms) */
            uint32_t t0 = HAL_GetTick();
            while (!adc2_done && HAL_GetTick() - t0 < 10) {}
            /* 降采样: 8192点 → 512点, 每16点取1个, >>4映射到8bit */
            for (uint16_t i = 0; i < PAGE1_PTS; i++)
              curve_data[i] = adc2_buf[i * 16] >> 4;
            HMI_tx_lock();
            HMI_curve_clear(CURVE_OBJ_ID, 0);
            HMI_curve_addt(CURVE_OBJ_ID, 0, curve_data, PAGE1_PTS);
            HMI_send_val("Vpp", (int)(adc_vpp_volt * 1000));
            HMI_send_val("Vrms", (int)(adc_vrms_volt * 1000));
            HMI_send_val("f", 0);
            HMI_tx_unlock();
            break;
          }
          case 2: /* 全屏波形 */
          {
            uint32_t t0 = HAL_GetTick();
            while (!adc2_done && HAL_GetTick() - t0 < 10) {}
            /* 降采样: 8192点 → 1024点, 每8点取1个 */
            for (uint16_t i = 0; i < PAGE2_PTS; i++)
              curve_data[i] = adc2_buf[i * 8] >> 4;
            HMI_tx_lock();
            HMI_curve_clear(CURVE_OBJ_ID, 0);
            HMI_curve_addt(CURVE_OBJ_ID, 0, curve_data, PAGE2_PTS);
            HMI_tx_unlock();
            break;
          }
          case 3: /* 频谱: FFT待实现 */
            break;
          case 4: /* 调试页 */
          {
            char dbg[32];
            snprintf(dbg, sizeof(dbg), "P%d R%d", current_page, rx_cnt);
            HMI_tx_lock();
            HMI_send_string("t0.txt", dbg);
            HMI_tx_unlock();
            break;
          }
          default: /* Home */
            HMI_tx_lock();
            HMI_send_string("t0.txt", "MCU OK");
            HMI_tx_unlock();
            break;
        }
      }
    }

    /* 电压转换: 16bit raw → mV */
    uint16_t vpp_mv = (uint16_t)(adc_vpp_volt * 1000.0f);
    uint16_t vrms_mv = (uint16_t)(adc_vrms_volt * 1000.0f);

    /* OLED显示:
       L1: P:xxxx R:xxxx  (Vpp/Vrms mV)
       L2: H:xxxx L:xxxx  (ADC2 max + min 0-4095)
       L3: A:xxxx         (ADC2 avg)
       L4: PGx RXxxx      (页面/RX计数)
    */
    OLED_ShowString(1,1,"P:");
    OLED_ShowNum(1,3,vpp_mv,4);
    OLED_ShowString(1,8,"R:");
    OLED_ShowNum(1,10,vrms_mv,4);

    OLED_ShowString(2,1,"H:");
    OLED_ShowNum(2,3,adc2_max,4);
    OLED_ShowString(2,7," L:");
    OLED_ShowNum(2,10,adc2_min,4);

    OLED_ShowString(3,1,"A:");
    OLED_ShowNum(3,3,adc2_avg,4);
    OLED_ShowString(3,7,"      ");

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

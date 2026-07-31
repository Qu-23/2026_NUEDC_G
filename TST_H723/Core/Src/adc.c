/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
#include "adc.h"
#include "tim.h"
#include "stm32h7xx_ll_adc.h"  /* LL_ADC_OVS_SHIFT_RIGHT_4 等过采样宏 */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

ADC_HandleTypeDef hadc2;
static DMA_HandleTypeDef hdma_adc2;

/* ADC2 init function - PA6/INP3, 12bit高速, TIM2 TRGO触发, DMA */
void MX_ADC2_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;  /* PLL2P=80MHz, 不分频, ADC时钟80MHz */
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T2_TRGO;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_ONESHOT;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
    Error_Handler();

  /* PA6 / ADC_CHANNEL_3, 最短采样时间1.5 cycles */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    Error_Handler();

  HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspInit 0 */

  /* USER CODE END ADC2_MspInit 0 */

  /** ADC时钟源配置 (PLL2 → 80MHz, ADC2 ASYNC_DIV1→80MHz)
   *  原由ADC1分支配置, ADC1移除后迁入此处 */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInitStruct.PLL2.PLL2M = 32;
    PeriphClkInitStruct.PLL2.PLL2N = 80;
    PeriphClkInitStruct.PLL2.PLL2P = 2;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_1;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC12共用时钟 (ADC1已移除, 但ADC2仍需使能) */
    __HAL_RCC_ADC12_CLK_ENABLE();

  /** ADC2 GPIO Configuration: PA6 ------> ADC2_INP3 (fast channel)
  */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC2 DMA: DMA1 Stream1, Channel2 (ADC2) */
    __HAL_RCC_DMA1_CLK_ENABLE();
    hdma_adc2.Instance = DMA1_Stream1;
    hdma_adc2.Init.Request = DMA_REQUEST_ADC2;
    hdma_adc2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc2.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc2.Init.Mode = DMA_NORMAL;
    hdma_adc2.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma_adc2.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc2) != HAL_OK)
      Error_Handler();

    __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc2);

    /* ADC2 NVIC (STM32H723中ADC1/ADC2共享ADC_IRQn) */
    HAL_NVIC_SetPriority(ADC_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    /* DMA1_Stream1 NVIC: 传输完成回调由DMA TC中断触发 */
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* USER CODE BEGIN ADC2_MspInit 1 */

  /* USER CODE END ADC2_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspDeInit 0 */

  /* USER CODE END ADC2_MspDeInit 0 */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);
    HAL_DMA_DeInit(&hdma_adc2);
    HAL_NVIC_DisableIRQ(ADC_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
  /* USER CODE BEGIN ADC2_MspDeInit 1 */

  /* USER CODE END ADC2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/* ===== ADC2 高速采样 (PA6/INP3, 4.03MHz, DMA) ===== */
/* 缓冲区放AXI SRAM (DMA_BUF段), 避免D-Cache问题 */
uint16_t adc2_buf[ADC2_BUF_SIZE] __attribute__((section("DMA_BUF")));
volatile uint8_t adc2_done = 1;
volatile uint32_t adc2_capture_ms = 0;  /* ADC2采样耗时(ms), 诊断用 */
static uint32_t adc2_start_tick = 0;

void ADC2_StartCapture(void)
{
  adc2_done = 0;
  adc2_start_tick = HAL_GetTick();
  /* 启动前Invalidate, 清除可能的脏Cache行 */
  SCB_InvalidateDCache_by_Addr((uint32_t *)adc2_buf, sizeof(adc2_buf));
  HAL_ADC_Stop_DMA(&hadc2);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_buf, ADC2_BUF_SIZE);
  HAL_TIM_Base_Start(&htim2);
}

/* ADC2 DMA完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC2)
  {
    HAL_TIM_Base_Stop(&htim2);
    adc2_capture_ms = HAL_GetTick() - adc2_start_tick;
    /* DMA完成后Invalidate, 确保CPU读到DMA写入的最新数据 */
    SCB_InvalidateDCache_by_Addr((uint32_t *)adc2_buf, sizeof(adc2_buf));
    adc2_done = 1;
  }
}

/* DMA1_Stream1中断服务: ADC2 DMA传输完成由此触发 */
void DMA1_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc2);
}

/* ADC中断服务 (STM32H723: ADC1/ADC2共享ADC_IRQn, 处理OVR等) */
volatile uint32_t ovr_cnt = 0;  /* OVR(过冲)计数, 诊断隔触发采样 */
void ADC_IRQHandler(void)
{
  if (__HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_OVR))
  {
    ovr_cnt++;
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_OVR);
  }
  HAL_ADC_IRQHandler(&hadc2);
}
/* USER CODE END 1 */

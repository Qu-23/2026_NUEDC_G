/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
uint8_t  USART_RX_BUF[USART_REC_LEN];
uint16_t USART_RX_STA = 0;
uint8_t rx_byte;
/* USER CODE END 0 */

UART_HandleTypeDef huart3;

/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN USART3_MspInit 1 */
    HAL_NVIC_SetPriority(USART3_IRQn, 3, 3);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
#include <stdio.h>
#include <string.h>

/* HMI发送：格式化后直接通过UART发送，不使用printf避免semi-hosting问题 */
#define HMI_END   "\xff\xff\xff"
#define HMI_BUF_SIZE 128

void HMI_send_string(char *name, char *showdata)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "%s=\"%s\"" HMI_END, name, showdata);
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HMI_send_number(char *name, int num)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "%s=%d" HMI_END, name, num);
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HMI_send_float(char *name, float num)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "%s=%d" HMI_END, name, (int)(num * 100));
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HMI_waveform_add(int curve_id, int ch, int value)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "add s%d.id,%d,%d" HMI_END, curve_id, ch, value);
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HMI_spectrum_add(int curve_id, int ch, int value)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "add s%d.id,%d,%d" HMI_END, curve_id, ch, value);
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    if ((USART_RX_STA & 0x8000) == 0)
    {
      if (USART_RX_STA & 0x4000)
      {
        if (rx_byte != 0x0a)
          USART_RX_STA = 0;
        else
          USART_RX_STA |= 0x8000;
      }
      else
      {
        if (rx_byte == 0x0d)
          USART_RX_STA |= 0x4000;
        else
        {
          USART_RX_BUF[USART_RX_STA & 0x3FFF] = rx_byte;
          USART_RX_STA++;
          if (USART_RX_STA > (USART_REC_LEN - 1))
            USART_RX_STA = 0;
        }
      }
    }
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    USART_RX_STA = 0;
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  }
}
/* USER CODE END 1 */

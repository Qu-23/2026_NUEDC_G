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
  int len = snprintf(buf, sizeof(buf), "%s.val=%d" HMI_END, name, (int)(num * 100));
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

void HMI_send_val(char *name, int num)
{
  char buf[HMI_BUF_SIZE];
  int len = snprintf(buf, sizeof(buf), "%s.val=%d" HMI_END, name, num);
  HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 100);
}

void HMI_curve_clear(const char *objid, uint8_t ch)
{
  char cmd[32];
  int len = snprintf(cmd, sizeof(cmd), "cle %s,%d" HMI_END, objid, ch);
  HAL_UART_Transmit(&huart3, (uint8_t *)cmd, len, 100);
}

/* addt透传: 批量发送曲线数据, 比逐点add快10倍以上 */
uint8_t HMI_curve_addt(const char *objid, uint8_t ch, const uint8_t *data, uint16_t qty)
{
  char cmd[32];
  uint32_t t0;
  uint8_t resp = 0;
  uint32_t cr1;

  if (qty > 1024 || qty == 0 || data == NULL) return 0;

  /* 保存RXNE中断状态并禁用, 防止TX回环字符污染RX状态机 */
  cr1 = huart3.Instance->CR1;
  __HAL_UART_DISABLE_IT(&huart3, UART_IT_RXNE);
  /* 清RDR残留 + ORE, 确保接收状态干净 */
  while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
  {
    volatile uint32_t tmp = huart3.Instance->RDR;
    (void)tmp;
  }
  __HAL_UART_CLEAR_OREFLAG(&huart3);

  /* 1. 发送addt指令 */
  {
    int len = snprintf(cmd, sizeof(cmd), "addt %s,%d,%d" HMI_END, objid, ch, qty);
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, len, 100);
  }

  /* 2. 轮询等待0xFE(透传就绪), 跳过TX回环字符, 超时50ms */
  t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < 50)
  {
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
      __HAL_UART_CLEAR_OREFLAG(&huart3);
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
      resp = (uint8_t)huart3.Instance->RDR;
      if (resp == 0xFE) break;
    }
  }
  if (resp != 0xFE)
  {
    /* 0xFE超时: 屏幕可能已进入透传模式, 发送qty字节0x00防止屏幕卡死 */
    static const uint8_t dummy[1024] = {0};
    HAL_UART_Transmit(&huart3, (uint8_t *)dummy, qty, 200);
    goto addt_done;
  }

  /* 3. 发送纯数据(qty字节, 无结束符) */
  HAL_UART_Transmit(&huart3, (uint8_t *)data, qty, 200);

  /* 4. 轮询等待0xFD(透传结束), 超时100ms */
  t0 = HAL_GetTick();
  resp = 0;
  while (HAL_GetTick() - t0 < 100)
  {
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
      __HAL_UART_CLEAR_OREFLAG(&huart3);
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
      resp = (uint8_t)huart3.Instance->RDR;
      if (resp == 0xFD) break;
    }
  }

addt_done:
  /* 清RDR残留 + 清ORE + 恢复RXNE中断状态 */
  while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
  {
    volatile uint32_t tmp = huart3.Instance->RDR;
    (void)tmp;
  }
  __HAL_UART_CLEAR_OREFLAG(&huart3);
  if (cr1 & USART_CR1_RXNEIE)
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);

  return (resp == 0xFD);
}

/* 批量发送期间加RX锁, 阻断TX回环伪帧污染RX状态机 */
void HMI_tx_lock(void)
{
  __HAL_UART_DISABLE_IT(&huart3, UART_IT_RXNE);
}

void HMI_tx_unlock(void)
{
  while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
  {
    volatile uint32_t tmp = huart3.Instance->RDR;
    (void)tmp;
  }
  __HAL_UART_CLEAR_OREFLAG(&huart3);
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
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

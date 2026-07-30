/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;

/* USER CODE BEGIN Private defines */
#define ADC2_BUF_SIZE  8192    /* ADC2高速采样缓冲区 (8192点@4MHz≈2ms) */
/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);

/* USER CODE BEGIN Prototypes */
extern uint16_t adc_vpp;      /* PA1/INP17 Vpp, 16bit raw */
extern uint16_t adc_vrms;     /* PA0/INP16 Vrms, 16bit raw */
extern float adc_vpp_volt;    /* Vpp电压值 (V) */
extern float adc_vrms_volt;   /* Vrms电压值 (V) */
void ADC_Read_Channels(void);

extern uint16_t adc2_buf[ADC2_BUF_SIZE];  /* PA6/INP3 ADC2 DMA缓冲区 */
extern volatile uint8_t adc2_done;        /* DMA完成标志 */
void ADC2_StartCapture(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */


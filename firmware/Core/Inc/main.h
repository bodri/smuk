/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

#include "stm32h5xx_nucleo.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MV_ON_Pin GPIO_PIN_2
#define MV_ON_GPIO_Port GPIOC
#define VRANGE_Pin GPIO_PIN_3
#define VRANGE_GPIO_Port GPIOC
#define IR1MA_Pin GPIO_PIN_0
#define IR1MA_GPIO_Port GPIOA
#define IR10MA_Pin GPIO_PIN_1
#define IR10MA_GPIO_Port GPIOA
#define IR100MA_Pin GPIO_PIN_2
#define IR100MA_GPIO_Port GPIOA
#define ICAL_Pin GPIO_PIN_4
#define ICAL_GPIO_Port GPIOC
#define IR2A_Pin GPIO_PIN_0
#define IR2A_GPIO_Port GPIOB
#define CALBUS_S0_Pin GPIO_PIN_12
#define CALBUS_S0_GPIO_Port GPIOB
#define CALBUS_S1_Pin GPIO_PIN_13
#define CALBUS_S1_GPIO_Port GPIOB
#define NDRDY_Pin GPIO_PIN_6
#define NDRDY_GPIO_Port GPIOC
#define NDRDY_EXTI_IRQn EXTI6_IRQn
#define VCAL_Pin GPIO_PIN_8
#define VCAL_GPIO_Port GPIOC
#define NRESET_Pin GPIO_PIN_9
#define NRESET_GPIO_Port GPIOC
#define IR100UA_Pin GPIO_PIN_12
#define IR100UA_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

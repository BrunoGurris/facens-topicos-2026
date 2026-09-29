/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "stm32c0xx_hal.h"

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
/* Entradas */
#define Det_Normal_Pin GPIO_PIN_0          /* detector de fim de curso NORMAL (EXTI0)  */
#define Det_Normal_GPIO_Port GPIOA
#define Det_Reversa_Pin GPIO_PIN_1         /* detector de fim de curso REVERSA (EXTI1) */
#define Det_Reversa_GPIO_Port GPIOA
#define Det_EXTI_IRQn EXTI0_1_IRQn
#define Ocupacao_Pin GPIO_PIN_4            /* circuito de via: 0 = trem sobre o AMV    */
#define Ocupacao_GPIO_Port GPIOA
#define Btn_Comando_Pin GPIO_PIN_10        /* pedido de manobra (alterna N/R), EXTI10  */
#define Btn_Comando_GPIO_Port GPIOA
#define Obstrucao_Pin GPIO_PIN_1           /* injecao de falha: 0 = agulha obstruida   */
#define Obstrucao_GPIO_Port GPIOB
#define Btn_Rearme_Pin GPIO_PIN_3          /* rearme apos falha/emergencia (polling)   */
#define Btn_Rearme_GPIO_Port GPIOB
#define Btn_Emergencia_Pin GPIO_PIN_13     /* parada de emergencia, EXTI13             */
#define Btn_Emergencia_GPIO_Port GPIOC
#define Btn_EXTI_IRQn EXTI4_15_IRQn

/* Saidas */
#define Heartbeat_Pin GPIO_PIN_5           /* LD4 da Nucleo                            */
#define Heartbeat_GPIO_Port GPIOA
#define Servo_Pin GPIO_PIN_6               /* maquina de chave: TIM3_CH1 (AF1)         */
#define Servo_GPIO_Port GPIOA
#define Sinal_Vermelho_Pin GPIO_PIN_0
#define Sinal_Vermelho_GPIO_Port GPIOB
#define Sinal_Amarelo_Pin GPIO_PIN_7
#define Sinal_Amarelo_GPIO_Port GPIOA
#define Sinal_Verde_Pin GPIO_PIN_6
#define Sinal_Verde_GPIO_Port GPIOB
#define Planta_DetN_Pin GPIO_PIN_4         /* simulacao: contato do detector NORMAL    */
#define Planta_DetN_GPIO_Port GPIOB        /*   (ligado por fio em PA0 no diagram.json) */
#define Planta_DetR_Pin GPIO_PIN_5         /* simulacao: contato do detector REVERSA   */
#define Planta_DetR_GPIO_Port GPIOB        /*   (ligado por fio em PA1)                 */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

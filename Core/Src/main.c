/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BLINK_PERIOD_DEFAULT_MS 500
#define BLINK_PERIOD_MIN_MS     20
#define BLINK_PERIOD_MAX_MS     10000
#define STATUS_HEARTBEAT_MS     1000   /* status periodico quando nao esta' piscando */
#define CMD_BUF_SIZE            32

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

static uint8_t  blink_enabled   = 1;
static uint32_t blink_period_ms = BLINK_PERIOD_DEFAULT_MS;
static uint32_t last_toggle_ms  = 0;
static uint32_t last_status_ms  = 0;

/* Linha de comando recebida pela USART2 (ver uart_poll_commands). */
static char    cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_len = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Envia uma string pela USART2 (monitor serial do Wokwi). */
static void uart_print(const char *msg)
{
  HAL_UART_Transmit(&huart2, (const uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* Envia o timer da placa (SysTick, ms desde o boot) e o estado atual.
 * Formato fixo "tick=<ms> led=<0|1> blink=<0|1> period=<ms>" -- e' o que
 * tools/plot_timer.py parseia. */
static void uart_print_status(void)
{
  char buf[64];
  last_status_ms = HAL_GetTick();
  snprintf(buf, sizeof(buf), "tick=%lu led=%d blink=%d period=%lu\r\n",
           (unsigned long)last_status_ms,
           HAL_GPIO_ReadPin(Led_GPIO_Port, Led_Pin) == GPIO_PIN_SET,
           blink_enabled,
           (unsigned long)blink_period_ms);
  uart_print(buf);
}

/* Executa uma linha de comando vinda da serial (pagina web ou monitor serial).
 * Comandos: led on|off|toggle, blink on|off, period <ms>, status, help. */
static void handle_command(char *cmd)
{
  char reply[64];

  if (strcmp(cmd, "led on") == 0)
    HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_SET);
  else if (strcmp(cmd, "led off") == 0)
    HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_RESET);
  else if (strcmp(cmd, "led toggle") == 0)
    HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
  else if (strcmp(cmd, "blink on") == 0)
    blink_enabled = 1;
  else if (strcmp(cmd, "blink off") == 0)
    blink_enabled = 0;
  else if (strncmp(cmd, "period ", 7) == 0)
  {
    long ms = atol(cmd + 7);
    if (ms < BLINK_PERIOD_MIN_MS || ms > BLINK_PERIOD_MAX_MS)
    {
      snprintf(reply, sizeof(reply), "err period fora de %d..%d ms\r\n",
               BLINK_PERIOD_MIN_MS, BLINK_PERIOD_MAX_MS);
      uart_print(reply);
      return;
    }
    blink_period_ms = (uint32_t)ms;
  }
  else if (strcmp(cmd, "status") == 0)
  {
    /* so' responde com a linha de status abaixo */
  }
  else if (strcmp(cmd, "help") == 0)
  {
    uart_print("comandos: led on|off|toggle, blink on|off, period <ms>, status, help\r\n");
    return;
  }
  else
  {
    snprintf(reply, sizeof(reply), "err comando desconhecido: %s\r\n", cmd);
    uart_print(reply);
    return;
  }

  snprintf(reply, sizeof(reply), "ok %s\r\n", cmd);
  uart_print(reply);
  uart_print_status();
}

/* Le a USART2 sem bloquear (polling do RXNE), monta uma linha e executa no '\n'. */
static void uart_poll_commands(void)
{
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE))
    __HAL_UART_CLEAR_OREFLAG(&huart2);

  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
  {
    char c = (char)(huart2.Instance->RDR & 0xFF);

    if (c == '\n' || c == '\r')
    {
      if (cmd_len > 0)
      {
        cmd_buf[cmd_len] = '\0';
        handle_command(cmd_buf);
        cmd_len = 0;
      }
    }
    else if (cmd_len < CMD_BUF_SIZE - 1)
    {
      cmd_buf[cmd_len++] = c;
    }
    else
    {
      cmd_len = 0;   /* linha grande demais: descarta */
      uart_print("err comando muito longo\r\n");
    }
  }
}

/* Chamado pelo HAL quando ocorre a interrupcao EXTI do botao (PC13). */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == User_Button_Pin)
  {
    HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
    uart_print("Botao pressionado!\r\n");
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  uart_print("Blink iniciado. Pressione o botao para inverter o LED.\r\n");
  uart_print("Envie 'help' pela serial para ver os comandos.\r\n");
  uart_print_status();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uart_poll_commands();

    uint32_t now = HAL_GetTick();
    if (blink_enabled)
    {
      if (now - last_toggle_ms >= blink_period_ms)
      {
        last_toggle_ms = now;
        HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
        uart_print_status();
      }
    }
    else if (now - last_status_ms >= STATUS_HEARTBEAT_MS)
    {
      uart_print_status();
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x20303E5D;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Led_GPIO_Port, Led_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : User_Button_Pin */
  GPIO_InitStruct.Pin = User_Button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(User_Button_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Led_Pin */
  GPIO_InitStruct.Pin = Led_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(Led_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
  * @param  line: assert_param error line source number
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

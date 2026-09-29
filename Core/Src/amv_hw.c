/*
 * amv_hw.c -- camada de hardware do controlador de AMV
 *
 *  TIM3  CH1 (PA6): PWM de 50 Hz para a maquina de chave (servo)
 *  TIM14          : contador livre de 1 us, usado para medir latencias
 *  EXTI           : detectores (PA0/PA1), botao de comando (PA10) e
 *                   emergencia (PC13) -> viram mensagens nas filas do RTOS
 *  USART2         : RX por interrupcao (1 byte por vez) e TX por interrupcao
 *
 * Os timers sao configurados direto nos registradores: o driver HAL_TIM nao
 * faz parte deste projeto e o que precisamos aqui sao poucas linhas.
 */
#include "main.h"
#include "amv.h"

extern UART_HandleTypeDef huart2;

/* Pulso do servo em us para 0 e 180 graus (padrao de servo de hobby). */
#define SERVO_US_0     1000u
#define SERVO_US_180   2000u

static uint8_t rx_byte;

/* ------------------------------------------------------------------ */
/* Base de tempo do HAL                                                */
/* ------------------------------------------------------------------ */

/* O SysTick e' do FreeRTOS (tick de 1 ms). O HAL nao reconfigura nada e
 * passa a usar o tick do kernel -- antes do escalonador ele vale 0, o que
 * so' desativa os timeouts do HAL durante o boot. */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  (void)TickPriority;
  return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
  return xTaskGetTickCount();
}

/* ------------------------------------------------------------------ */
/* Inicializacao                                                       */
/* ------------------------------------------------------------------ */

void amv_hw_init(void)
{
  const uint32_t psc_1mhz = SystemCoreClock / 1000000u - 1u;

  /* TIM14: cronometro livre de 1 us (estoura a cada 65,536 ms). */
  __HAL_RCC_TIM14_CLK_ENABLE();
  TIM14->PSC = psc_1mhz;
  TIM14->ARR = 0xFFFF;
  TIM14->EGR = TIM_EGR_UG;
  TIM14->CR1 = TIM_CR1_CEN;

  /* TIM3: 1 MHz / 20000 = 50 Hz, PWM modo 1 no canal 1. */
  __HAL_RCC_TIM3_CLK_ENABLE();
  TIM3->PSC   = psc_1mhz;
  TIM3->ARR   = 20000u - 1u;
  TIM3->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;
  TIM3->CCER  = TIM_CCER_CC1E;
  amv_hw_servo(ANG_INICIAL_CDEG);
  TIM3->EGR   = TIM_EGR_UG;
  TIM3->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;

  /* USART2 por interrupcao: abaixo dos botoes/detectores. */
  HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

uint16_t amv_us(void)
{
  return (uint16_t)TIM14->CNT;
}

/* ------------------------------------------------------------------ */
/* Atuadores                                                           */
/* ------------------------------------------------------------------ */

void amv_hw_servo(uint16_t cdeg)
{
  TIM3->CCR1 = SERVO_US_0 + (uint32_t)cdeg * (SERVO_US_180 - SERVO_US_0) / 18000u;
}

void amv_hw_sinal(amv_sinal_t s)
{
  HAL_GPIO_WritePin(Sinal_Vermelho_GPIO_Port, Sinal_Vermelho_Pin,
                    s == SINAL_VERMELHO ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Sinal_Amarelo_GPIO_Port, Sinal_Amarelo_Pin,
                    s == SINAL_AMARELO ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Sinal_Verde_GPIO_Port, Sinal_Verde_Pin,
                    s == SINAL_VERDE ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void amv_hw_heartbeat(void)
{
  HAL_GPIO_TogglePin(Heartbeat_GPIO_Port, Heartbeat_Pin);
}

void amv_hw_planta_detector(amv_pos_t p)
{
  HAL_GPIO_WritePin(Planta_DetN_GPIO_Port, Planta_DetN_Pin,
                    p == POS_NORMAL ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Planta_DetR_GPIO_Port, Planta_DetR_Pin,
                    p == POS_REVERSA ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Chamado em falha grave (assert, stack overflow, Error_Handler):
 * sinal vermelho e motor parado onde estiver. Nao usa nada do RTOS. */
void amv_hw_estado_seguro(void)
{
  amv_hw_sinal(SINAL_VERMELHO);
  TIM3->CR1 &= ~TIM_CR1_CEN;
}

/* ------------------------------------------------------------------ */
/* Sensores                                                            */
/* ------------------------------------------------------------------ */

amv_pos_t amv_hw_detectores(void)
{
  uint8_t n = HAL_GPIO_ReadPin(Det_Normal_GPIO_Port, Det_Normal_Pin) == GPIO_PIN_SET;
  uint8_t r = HAL_GPIO_ReadPin(Det_Reversa_GPIO_Port, Det_Reversa_Pin) == GPIO_PIN_SET;
  if (n && r) return POS_DUPLA;
  if (n)      return POS_NORMAL;
  if (r)      return POS_REVERSA;
  return POS_NENHUMA;
}

/* Chaves deslizantes ligam o pino ao GND quando ativas (pull-up interno). */
uint8_t amv_hw_ocupado(void)
{
  return HAL_GPIO_ReadPin(Ocupacao_GPIO_Port, Ocupacao_Pin) == GPIO_PIN_RESET
         || g_ocupado_virtual;
}

uint8_t amv_hw_obstruido(void)
{
  return HAL_GPIO_ReadPin(Obstrucao_GPIO_Port, Obstrucao_Pin) == GPIO_PIN_RESET
         || g_obstruido_virtual;
}

uint8_t amv_hw_botao_rearme(void)
{
  return HAL_GPIO_ReadPin(Btn_Rearme_GPIO_Port, Btn_Rearme_Pin) == GPIO_PIN_RESET;
}

/* ------------------------------------------------------------------ */
/* Interrupcoes -> filas do RTOS                                       */
/* ------------------------------------------------------------------ */

/* A ISR so' carimba o tempo e posta a mensagem: todo o processamento fica
 * nas tarefas. portYIELD_FROM_ISR troca de contexto na saida da interrupcao
 * se a mensagem acordou uma tarefa mais prioritaria que a interrompida. */
static void isr_detector(void)
{
  if (g_q_seg == NULL) return;   /* IRQ antes de amv_start() criar as filas */
  BaseType_t acordou = pdFALSE;
  seg_evt_t ev = { .tipo = SEG_DETECTOR, .t_us = amv_us() };
  xQueueSendFromISR(g_q_seg, &ev, &acordou);
  portYIELD_FROM_ISR(acordou);
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == Det_Normal_Pin || GPIO_Pin == Det_Reversa_Pin)
    isr_detector();
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  static TickType_t ult_comando, ult_emerg;
  BaseType_t acordou = pdFALSE;
  TickType_t agora = xTaskGetTickCountFromISR();
  uint16_t t_us = amv_us();

  if (g_q_seg == NULL || g_q_cmd == NULL) return;   /* boot: filas ainda nao existem */

  if (GPIO_Pin == Det_Normal_Pin || GPIO_Pin == Det_Reversa_Pin)
  {
    isr_detector();
  }
  else if (GPIO_Pin == Btn_Emergencia_Pin)
  {
    if (agora - ult_emerg >= pdMS_TO_TICKS(T_DEBOUNCE_MS))
    {
      ult_emerg = agora;
      seg_evt_t ev = { .tipo = SEG_EMERGENCIA, .t_us = t_us };
      xQueueSendFromISR(g_q_seg, &ev, &acordou);
    }
  }
  else if (GPIO_Pin == Btn_Comando_Pin)
  {
    if (agora - ult_comando >= pdMS_TO_TICKS(T_DEBOUNCE_MS))
    {
      ult_comando = agora;
      cmd_t c = { .tipo = CMD_ALTERNAR, .origem = ORIG_BOTAO, .t_us = t_us };
      xQueueSendFromISR(g_q_cmd, &c, &acordou);
    }
  }
  portYIELD_FROM_ISR(acordou);
}

/* UART: cada byte recebido vai para g_q_rx; o fim de uma transmissao
 * libera o semaforo g_sem_tx que a tarefa de comunicacao esta' esperando. */
void amv_hw_uart_rx_start(void)
{
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t acordou = pdFALSE;
  xQueueSendFromISR(g_q_rx, &rx_byte, &acordou);
  HAL_UART_Receive_IT(huart, &rx_byte, 1);
  portYIELD_FROM_ISR(acordou);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  HAL_UART_Receive_IT(huart, &rx_byte, 1);   /* overrun etc.: so' rearma */
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t acordou = pdFALSE;
  (void)huart;
  xSemaphoreGiveFromISR(g_sem_tx, &acordou);
  portYIELD_FROM_ISR(acordou);
}

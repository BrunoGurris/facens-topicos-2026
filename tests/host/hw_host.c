/*
 * Hardware falso para rodar as tarefas do AMV no PC (substitui amv_hw.c).
 *
 * - GPIO: os contatos que a tPlanta gera em PB4/PB5 chegam em PA0/PA1 como
 *   no fio do diagram.json, e cada borda vira um evento SEG_DETECTOR na fila
 *   da tSeg -- o mesmo que a ISR da EXTI faz na placa.
 * - UART: o que a tComm transmite vai para o stdout (linhas "st"/"tr" so'
 *   com HIL_VERBOSE=1, para o log ficar legivel) e o fim da transmissao
 *   libera o semaforo g_sem_tx, como o HAL_UART_TxCpltCallback.
 * - hil_fio_r_quebrado simula o fio PB5 -> PA1 rompido (teste do diagnostico).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "main.h"
#include "amv.h"

UART_HandleTypeDef huart2;
uint32_t SystemCoreClock = 48000000u;
volatile uint8_t hil_fio_r_quebrado;

static volatile uint8_t pa0, pa1;

void Error_Handler(void)          { puts("HIL: Error_Handler"); exit(2); }
void amv_hw_estado_seguro(void)   { puts("HIL: estado seguro (assert/stack overflow)"); exit(4); }

uint16_t amv_us(void)
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint16_t)(t.tv_sec * 1000000ull + t.tv_nsec / 1000);
}

void      amv_hw_init(void)            {}
void      amv_hw_servo(uint16_t cdeg)  { (void)cdeg; }
void      amv_hw_sinal(amv_sinal_t s)  { (void)s; }
void      amv_hw_heartbeat(void)       {}
void      amv_hw_uart_rx_start(void)   {}
uint8_t   amv_hw_botao_rearme(void)    { return 0; }
uint8_t   amv_hw_ocupado(void)         { return g_ocupado_virtual || g_trem_ocupado; }
uint8_t   amv_hw_obstruido(void)       { return g_obstruido_virtual; }

amv_pos_t amv_hw_detectores(void)
{
  if (pa0 && pa1) return POS_DUPLA;
  return pa0 ? POS_NORMAL : pa1 ? POS_REVERSA : POS_NENHUMA;
}

static void borda_detector(void)
{
  seg_evt_t ev = { .tipo = SEG_DETECTOR, .t_us = amv_us() };
  xQueueSend(g_q_seg, &ev, 0);
}

void amv_hw_planta_detector(amv_pos_t p)
{
  uint8_t n = p == POS_NORMAL;
  uint8_t r = p == POS_REVERSA && !hil_fio_r_quebrado;
  if (n != pa0) { pa0 = n; borda_detector(); }
  if (r != pa1) { pa1 = r; borda_detector(); }
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, const uint8_t *d, uint16_t n)
{
  (void)h;
  int periodica = !strncmp((const char *)d, "st ", 3) || !strncmp((const char *)d, "tr ", 3);
  if (!periodica || getenv("HIL_VERBOSE"))
  {
    fwrite(d, 1, n, stdout);
    fflush(stdout);
  }
  xSemaphoreGive(g_sem_tx);
  return HAL_OK;
}

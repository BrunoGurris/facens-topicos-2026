/*
 * amv_comm.c -- tarefa de comunicacao (prioridade mais baixa)
 *
 * Unica tarefa que usa a USART2 para transmitir, entao a serial nao precisa
 * de mutex: as outras tarefas mandam mensagens pela fila g_q_log e seguem
 * em frente (nunca esperam pela serial). A transmissao e' por interrupcao:
 * a tarefa dorme no semaforo g_sem_tx enquanto os bytes saem.
 *
 * Protocolo (uma linha por mensagem, lido por tools/painel_amv.py):
 *   st t=<ms> est=<estado> cmd=<N|R> det=<N|R|-|NR> sig=<vermelho|amarelo|verde>
 *      ang=<cdeg> occ=<0|1> obs=<0|1> falha=<tipo>            (a cada 250 ms)
 *   ev t=<ms> <evento> chave=valor...                          (eventos)
 *   tm lat_emg=<ult>/<max> lat_perda=... lat_cmd=... manobra=... jit=...
 *   ok <comando> | err <motivo>                                (respostas)
 *
 * Comandos: n|normal, r|reversa, t|alternar, emg, rearme, occ 0|1, obs 0|1,
 *           status, stats, help
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "amv.h"

extern UART_HandleTypeDef huart2;

volatile uint8_t g_ocupado_virtual;
volatile uint8_t g_obstruido_virtual;

#define LINHA_MAX 24

static const char *const NOME_EST[]    = { "movendo", "travado", "falha", "emergencia" };
static const char *const NOME_POS[]    = { "-", "N", "R", "NR" };
static const char *const NOME_SINAL[]  = { "vermelho", "amarelo", "verde" };
static const char *const NOME_FALHA[]  = { "nenhuma", "timeout", "perda_det", "det_dupla" };
static const char *const NOME_REJ[]    = { "ocupado", "em_movimento", "bloqueado", "ja_na_posicao" };
static const char *const NOME_ORIGEM[] = { "serial", "botao", "sistema" };

static void envia(const char *s)
{
  if (HAL_UART_Transmit_IT(&huart2, (const uint8_t *)s, strlen(s)) == HAL_OK)
    xSemaphoreTake(g_sem_tx, pdMS_TO_TICKS(100));
}

static void envia_status(void)
{
  amv_status_t s;
  char buf[128];
  amv_status_copia(&s);
  snprintf(buf, sizeof(buf),
           "st t=%lu est=%s cmd=%s det=%s sig=%s ang=%u occ=%u obs=%u falha=%s\r\n",
           (unsigned long)xTaskGetTickCount(), NOME_EST[s.estado], NOME_POS[s.comandada],
           NOME_POS[s.detectada], NOME_SINAL[s.sinal], (unsigned)g_angulo_cdeg,
           s.ocupado, s.obstruido, NOME_FALHA[s.falha]);
  envia(buf);
}

static void envia_tempos(void)
{
  amv_stats_t st;
  char buf[160];
  taskENTER_CRITICAL();
  st = g_stats;
  taskEXIT_CRITICAL();
  snprintf(buf, sizeof(buf),
           "tm lat_emg=%u/%u lat_perda=%u/%u lat_cmd=%u/%u manobra=%lu/%lu jit=%u "
           "n=%lu falhas=%lu rej=%lu\r\n",
           st.lat_emerg_us, st.lat_emerg_max_us, st.lat_perda_us, st.lat_perda_max_us,
           st.lat_cmd_us, st.lat_cmd_max_us,
           (unsigned long)st.manobra_ms, (unsigned long)st.manobra_max_ms,
           st.jitter_motor_max_us, (unsigned long)st.manobras,
           (unsigned long)st.falhas, (unsigned long)st.rejeicoes);
  envia(buf);
}

/* Pilha livre (palavras) de cada tarefa: base para dimensionar as pilhas. */
static void envia_pilhas(void)
{
  char buf[128];
  int  n = snprintf(buf, sizeof(buf), "stk");
  const char *nome;
  for (uint8_t i = 0; i < 8; i++)
  {
    uint16_t livre = amv_stack_livre(i, &nome);
    if (livre == 0) break;
    n += snprintf(buf + n, sizeof(buf) - n, " %s=%u", nome, livre);
  }
  snprintf(buf + n, sizeof(buf) - n, "\r\n");
  envia(buf);
}

static void envia_log(const log_msg_t *m)
{
  char buf[112];
  unsigned long t = (unsigned long)m->t_ms;

  switch (m->tipo)
  {
  case LOG_PEDIDO:
    snprintf(buf, sizeof(buf), "ev t=%lu pedido alvo=%s origem=%s lat_us=%lu\r\n",
             t, NOME_POS[m->a], NOME_ORIGEM[m->b], (unsigned long)m->v);
    break;
  case LOG_REJEITADO:
    snprintf(buf, sizeof(buf), "ev t=%lu rejeitado motivo=%s origem=%s\r\n",
             t, NOME_REJ[m->a], NOME_ORIGEM[m->b]);
    break;
  case LOG_TRAVADO:
    snprintf(buf, sizeof(buf), "ev t=%lu travado pos=%s manobra_ms=%lu\r\n",
             t, NOME_POS[m->a], (unsigned long)m->v);
    break;
  case LOG_FALHA:
    snprintf(buf, sizeof(buf), m->a == FALHA_TIMEOUT
                               ? "ev t=%lu falha tipo=%s apos_ms=%lu\r\n"
                               : "ev t=%lu falha tipo=%s lat_us=%lu\r\n",
             t, NOME_FALHA[m->a], (unsigned long)m->v);
    break;
  case LOG_EMERGENCIA:
    snprintf(buf, sizeof(buf), "ev t=%lu emergencia lat_us=%lu\r\n", t, (unsigned long)m->v);
    break;
  case LOG_REARME:
    snprintf(buf, sizeof(buf), "ev t=%lu rearme origem=%s\r\n", t, NOME_ORIGEM[m->b]);
    break;
  default:
    return;
  }
  envia(buf);
  envia_tempos();
}

static void pede(uint8_t tipo)
{
  cmd_t c = { .tipo = tipo, .origem = ORIG_SERIAL, .t_us = amv_us() };
  xQueueSend(g_q_cmd, &c, 0);
}

static void executa(char *cmd)
{
  char resp[64];

  if (!strcmp(cmd, "n") || !strcmp(cmd, "normal"))        pede(CMD_NORMAL);
  else if (!strcmp(cmd, "r") || !strcmp(cmd, "reversa"))  pede(CMD_REVERSA);
  else if (!strcmp(cmd, "t") || !strcmp(cmd, "alternar")) pede(CMD_ALTERNAR);
  else if (!strcmp(cmd, "rearme") || !strcmp(cmd, "reset")) pede(CMD_REARME);
  else if (!strcmp(cmd, "emg"))
  {
    seg_evt_t ev = { .tipo = SEG_EMERGENCIA, .t_us = amv_us() };
    xQueueSend(g_q_seg, &ev, 0);
  }
  else if (!strncmp(cmd, "occ ", 4))  g_ocupado_virtual   = atoi(cmd + 4) != 0;
  else if (!strncmp(cmd, "obs ", 4))  g_obstruido_virtual = atoi(cmd + 4) != 0;
  else if (!strcmp(cmd, "status"))    { envia_status(); return; }
  else if (!strcmp(cmd, "stats"))     { envia_tempos(); envia_pilhas(); return; }
  else if (!strcmp(cmd, "help"))
  {
    envia("comandos: n|normal, r|reversa, t|alternar, emg, rearme, "
          "occ 0|1, obs 0|1, status, stats, help\r\n");
    return;
  }
  else
  {
    snprintf(resp, sizeof(resp), "err comando desconhecido: %.20s\r\n", cmd);
    envia(resp);
    return;
  }
  snprintf(resp, sizeof(resp), "ok %s\r\n", cmd);
  envia(resp);
}

void tarefa_comm(void *arg)
{
  (void)arg;
  char       linha[LINHA_MAX];
  uint8_t    len = 0, ch;
  log_msg_t  m;
  TickType_t prox_status = xTaskGetTickCount();

  amv_hw_uart_rx_start();
  envia("\r\nAMV: controle de desvio ferroviario (FreeRTOS " tskKERNEL_VERSION_NUMBER ")\r\n"
        "Envie 'help' para ver os comandos.\r\n");

  for (;;)
  {
    /* Dorme ate' chegar log (ou no maximo 10 ms, para olhar a RX). */
    if (xQueueReceive(g_q_log, &m, pdMS_TO_TICKS(10)) == pdTRUE)
      envia_log(&m);

    while (xQueueReceive(g_q_rx, &ch, 0) == pdTRUE)
    {
      if (ch == '\n' || ch == '\r')
      {
        if (len > 0)
        {
          linha[len] = '\0';
          executa(linha);
          len = 0;
        }
      }
      else if (len < LINHA_MAX - 1)
      {
        linha[len++] = (char)ch;
      }
      else
      {
        len = 0;
        envia("err comando muito longo\r\n");
      }
    }

    if ((int32_t)(xTaskGetTickCount() - prox_status) >= 0)
    {
      prox_status += pdMS_TO_TICKS(T_STATUS_PERIODO_MS);
      envia_status();
    }
  }
}

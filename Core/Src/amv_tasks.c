/*
 * amv_tasks.c -- tarefas do FreeRTOS do controlador de AMV
 *
 *  Tarefa     Prio  Ativacao                    Funcao
 *  ---------  ----  --------------------------  -------------------------------
 *  tSeg        5    evento (g_q_seg, de ISR)    emergencia e supervisao dos
 *                                               detectores -> sinal vermelho
 *  tMotor      4    evento + periodica 20 ms    rampa da maquina de chave
 *  tInter      3    evento (g_q_cmd) + watchdog maquina de estados do
 *                                               intertravamento
 *  tSinal      2    periodica 50 ms             aspecto do sinal, heartbeat,
 *                                               botao de rearme, ocupacao
 *  tPlanta     2    periodica 10 ms             SIMULACAO dos contatos dos
 *                                               detectores (nao existe na
 *                                               placa real)
 *  tComm       1    evento (g_q_log/g_q_rx)     protocolo serial / telemetria
 *
 *  Recursos: 5 filas, 1 mutex (g_mtx_amv), 1 semaforo binario (g_sem_tx).
 *  Tudo alocado estaticamente.
 */
#include <string.h>
#include "main.h"
#include "amv.h"

/* ------------------------------------------------------------------ */
/* Objetos do RTOS                                                     */
/* ------------------------------------------------------------------ */

#define Q_SEG_LEN    8
#define Q_CMD_LEN    8
#define Q_MOTOR_LEN  4
#define Q_LOG_LEN    12
#define Q_RX_LEN     32

QueueHandle_t     g_q_seg, g_q_cmd, g_q_motor, g_q_log, g_q_rx;
SemaphoreHandle_t g_mtx_amv, g_sem_tx;

amv_status_t      g_amv = {
  .estado = EST_MOVENDO, .comandada = POS_NORMAL, .detectada = POS_NENHUMA,
  .sinal = SINAL_VERMELHO, .falha = FALHA_NENHUMA,
};
amv_stats_t       g_stats;
volatile uint16_t g_angulo_cdeg = ANG_INICIAL_CDEG;

/* Macro para criar uma fila estatica: buffer + estrutura de controle. */
#define FILA_ESTATICA(nome, len, tipo)                                   \
  static uint8_t       nome##_buf[(len) * sizeof(tipo)];                 \
  static StaticQueue_t nome##_ctl

FILA_ESTATICA(q_seg,   Q_SEG_LEN,   seg_evt_t);
FILA_ESTATICA(q_cmd,   Q_CMD_LEN,   cmd_t);
FILA_ESTATICA(q_motor, Q_MOTOR_LEN, motor_cmd_t);
FILA_ESTATICA(q_log,   Q_LOG_LEN,   log_msg_t);
FILA_ESTATICA(q_rx,    Q_RX_LEN,    uint8_t);
static StaticSemaphore_t mtx_amv_ctl, sem_tx_ctl;

/* Pilhas em palavras de 32 bits */
#define STK_SEG     128
#define STK_MOTOR   112
#define STK_INTER   128
#define STK_SINAL   96
#define STK_PLANTA  96
#define STK_COMM    256

typedef struct {
  TaskFunction_t fn;
  const char    *nome;
  uint16_t       stk_len;
  UBaseType_t    prio;
  StackType_t   *stk;
  StaticTask_t  *tcb;
  TaskHandle_t   handle;
} tarefa_def_t;

#define TAREFA(fn, nome, stk, prio)                                         \
  { fn, nome, stk, prio, (StackType_t[stk]){0}, &(StaticTask_t){0}, NULL }

static void tarefa_seguranca(void *arg);
static void tarefa_motor(void *arg);
static void tarefa_intertravamento(void *arg);
static void tarefa_sinal(void *arg);
static void tarefa_planta(void *arg);

static tarefa_def_t tarefas[] = {
  TAREFA(tarefa_seguranca,       "tSeg",    STK_SEG,    PRIO_SEGURANCA),
  TAREFA(tarefa_motor,           "tMotor",  STK_MOTOR,  PRIO_MOTOR),
  TAREFA(tarefa_intertravamento, "tInter",  STK_INTER,  PRIO_INTERTRAV),
  TAREFA(tarefa_sinal,           "tSinal",  STK_SINAL,  PRIO_SINAL),
  TAREFA(tarefa_planta,          "tPlanta", STK_PLANTA, PRIO_PLANTA),
  TAREFA(tarefa_comm,            "tComm",   STK_COMM,   PRIO_COMM),
};
#define N_TAREFAS (sizeof(tarefas) / sizeof(tarefas[0]))

void amv_start(void)
{
  g_q_seg   = xQueueCreateStatic(Q_SEG_LEN,   sizeof(seg_evt_t),   q_seg_buf,   &q_seg_ctl);
  g_q_cmd   = xQueueCreateStatic(Q_CMD_LEN,   sizeof(cmd_t),       q_cmd_buf,   &q_cmd_ctl);
  g_q_motor = xQueueCreateStatic(Q_MOTOR_LEN, sizeof(motor_cmd_t), q_motor_buf, &q_motor_ctl);
  g_q_log   = xQueueCreateStatic(Q_LOG_LEN,   sizeof(log_msg_t),   q_log_buf,   &q_log_ctl);
  g_q_rx    = xQueueCreateStatic(Q_RX_LEN,    sizeof(uint8_t),     q_rx_buf,    &q_rx_ctl);
  g_mtx_amv = xSemaphoreCreateMutexStatic(&mtx_amv_ctl);
  g_sem_tx  = xSemaphoreCreateBinaryStatic(&sem_tx_ctl);

  for (unsigned i = 0; i < N_TAREFAS; i++)
  {
    tarefa_def_t *t = &tarefas[i];
    t->handle = xTaskCreateStatic(t->fn, t->nome, t->stk_len, NULL, t->prio, t->stk, t->tcb);
  }

  vTaskStartScheduler();   /* nao retorna */
  Error_Handler();
}

uint16_t amv_stack_livre(uint8_t idx, const char **nome)
{
  if (idx >= N_TAREFAS) return 0;
  *nome = tarefas[idx].nome;
  return (uint16_t)uxTaskGetStackHighWaterMark(tarefas[idx].handle);
}

/* ------------------------------------------------------------------ */
/* Utilitarios                                                         */
/* ------------------------------------------------------------------ */

void amv_log(uint8_t tipo, uint8_t a, uint8_t b, uint32_t v)
{
  log_msg_t m = { .tipo = tipo, .a = a, .b = b, .t_ms = xTaskGetTickCount(), .v = v };
  /* Nao bloqueia: se a serial estiver atrasada, perde-se o log, nunca o prazo. */
  xQueueSend(g_q_log, &m, 0);
}

void amv_status_copia(amv_status_t *dst)
{
  xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
  *dst = g_amv;
  xSemaphoreGive(g_mtx_amv);
}

static uint16_t us_desde(uint16_t t0)
{
  return (uint16_t)(amv_us() - t0);   /* aritmetica modulo 2^16 */
}

static void registra_max16(uint16_t *ult, uint16_t *max, uint16_t v)
{
  taskENTER_CRITICAL();
  *ult = v;
  if (v > *max) *max = v;
  taskEXIT_CRITICAL();
}

static uint16_t angulo_alvo(amv_pos_t p)
{
  return p == POS_REVERSA ? ANG_REVERSA_CDEG : ANG_NORMAL_CDEG;
}

/* Aspecto do sinal em funcao do estado. Chamar com g_mtx_amv. */
static amv_sinal_t aspecto(const amv_status_t *s)
{
  if (s->estado != EST_TRAVADO || s->detectada != s->comandada)
    return SINAL_VERMELHO;
  return s->comandada == POS_NORMAL ? SINAL_VERDE : SINAL_AMARELO;
}

/* Leva o AMV a um estado restritivo: muda o estado, apaga o sinal e para o
 * motor. Chamar com g_mtx_amv. O sinal e' escrito aqui mesmo (e nao so' na
 * proxima rodada do tSinal) para a latencia ficar em microssegundos. */
static void restringe(amv_estado_t est, amv_falha_t falha)
{
  motor_cmd_t parar = { .tipo = MOTOR_PARAR };
  g_amv.estado = est;
  g_amv.falha  = falha;
  g_amv.sinal  = SINAL_VERMELHO;
  amv_hw_sinal(SINAL_VERMELHO);
  xQueueSendToFront(g_q_motor, &parar, 0);
}

/* ------------------------------------------------------------------ */
/* tSeg -- prioridade maxima                                           */
/* ------------------------------------------------------------------ */

static void tarefa_seguranca(void *arg)
{
  (void)arg;
  seg_evt_t ev;

  for (;;)
  {
    xQueueReceive(g_q_seg, &ev, portMAX_DELAY);

    if (ev.tipo == SEG_EMERGENCIA)
    {
      xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
      restringe(EST_EMERGENCIA, FALHA_NENHUMA);
      xSemaphoreGive(g_mtx_amv);

      uint16_t lat = us_desde(ev.t_us);
      registra_max16(&g_stats.lat_emerg_us, &g_stats.lat_emerg_max_us, lat);
      amv_log(LOG_EMERGENCIA, 0, 0, lat);
      continue;
    }

    /* SEG_DETECTOR: algum contato mudou. Le o estado real dos dois. */
    amv_pos_t det = amv_hw_detectores();
    uint8_t   confirma = 0, falhou = 0;
    amv_falha_t falha = FALHA_NENHUMA;

    xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
    g_amv.detectada = det;
    if (det == POS_DUPLA && g_amv.estado != EST_EMERGENCIA)
    {
      falha = FALHA_DET_DUPLA;
    }
    else if (g_amv.estado == EST_TRAVADO && det != g_amv.comandada)
    {
      falha = FALHA_PERDA_DET;       /* agulha se abriu com a via liberada */
    }
    else if (g_amv.estado == EST_MOVENDO && det == g_amv.comandada)
    {
      confirma = 1;
    }
    if (falha != FALHA_NENHUMA && g_amv.estado != EST_FALHA)
    {
      restringe(EST_FALHA, falha);
      falhou = 1;
    }
    xSemaphoreGive(g_mtx_amv);

    if (falhou)
    {
      uint16_t lat = us_desde(ev.t_us);
      registra_max16(&g_stats.lat_perda_us, &g_stats.lat_perda_max_us, lat);
      taskENTER_CRITICAL();
      g_stats.falhas++;
      taskEXIT_CRITICAL();
      amv_log(LOG_FALHA, falha, 0, lat);
    }
    if (confirma)
    {
      cmd_t c = { .tipo = CMD_DET_OK, .origem = ORIG_SISTEMA, .t_us = ev.t_us };
      xQueueSend(g_q_cmd, &c, 0);
    }
  }
}

/* ------------------------------------------------------------------ */
/* tMotor -- rampa da maquina de chave                                  */
/* ------------------------------------------------------------------ */

static void tarefa_motor(void *arg)
{
  (void)arg;
  motor_cmd_t m;
  uint16_t atual = g_angulo_cdeg;

  for (;;)
  {
    /* Parado: dorme ate' chegar um comando (nao gasta CPU). */
    xQueueReceive(g_q_motor, &m, portMAX_DELAY);
    if (m.tipo != MOTOR_MOVER) continue;

    uint16_t   alvo    = m.alvo_cdeg;
    TickType_t proximo = xTaskGetTickCount();
    uint16_t   t_ant   = amv_us();

    while (atual != alvo)
    {
      /* Periodica de 20 ms, sem deriva: o proximo instante e' calculado a
       * partir do anterior, nao de "agora". */
      vTaskDelayUntil(&proximo, pdMS_TO_TICKS(T_MOTOR_PERIODO_MS));

      uint16_t t = amv_us();
      int32_t  desvio = (int32_t)(uint16_t)(t - t_ant) - T_MOTOR_PERIODO_MS * 1000;
      t_ant = t;
      if (desvio < 0) desvio = -desvio;
      taskENTER_CRITICAL();
      if ((uint16_t)desvio > g_stats.jitter_motor_max_us)
        g_stats.jitter_motor_max_us = (uint16_t)desvio;
      taskEXIT_CRITICAL();

      /* Comando novo no meio do movimento (PARAR tem prioridade: vem na
       * frente da fila). */
      if (xQueueReceive(g_q_motor, &m, 0) == pdTRUE)
      {
        if (m.tipo == MOTOR_PARAR) break;
        alvo = m.alvo_cdeg;
      }

      if (atual < alvo)
        atual = (alvo - atual > ANG_PASSO_CDEG) ? atual + ANG_PASSO_CDEG : alvo;
      else
        atual = (atual - alvo > ANG_PASSO_CDEG) ? atual - ANG_PASSO_CDEG : alvo;

      amv_hw_servo(atual);
      g_angulo_cdeg = atual;
    }
  }
}

/* ------------------------------------------------------------------ */
/* tInter -- intertravamento                                            */
/* ------------------------------------------------------------------ */

/* Inicia uma manobra para 'alvo'. Chamar com g_mtx_amv. */
static void inicia_manobra(amv_pos_t alvo, TickType_t *inicio)
{
  motor_cmd_t mv = { .tipo = MOTOR_MOVER, .alvo_cdeg = angulo_alvo(alvo) };
  g_amv.estado    = EST_MOVENDO;
  g_amv.comandada = alvo;
  g_amv.falha     = FALHA_NENHUMA;
  g_amv.sinal     = SINAL_VERMELHO;
  amv_hw_sinal(SINAL_VERMELHO);           /* sinal fecha ANTES do motor mexer */
  *inicio = xTaskGetTickCount();
  xQueueSend(g_q_motor, &mv, 0);
}

static void tarefa_intertravamento(void *arg)
{
  (void)arg;
  cmd_t      c;
  TickType_t inicio;

  /* Boot: posicao desconhecida -> leva o AMV para NORMAL e verifica. */
  xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
  inicia_manobra(POS_NORMAL, &inicio);
  xSemaphoreGive(g_mtx_amv);

  for (;;)
  {
    /* Watchdog de manobra: enquanto MOVENDO, espera no maximo o que falta
     * do prazo T_WATCHDOG_MS; fora disso, espera indefinidamente. */
    TickType_t espera = portMAX_DELAY;
    xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
    if (g_amv.estado == EST_MOVENDO)
    {
      TickType_t passado = xTaskGetTickCount() - inicio;
      TickType_t limite  = pdMS_TO_TICKS(T_WATCHDOG_MS);
      espera = passado >= limite ? 0 : limite - passado;
    }
    xSemaphoreGive(g_mtx_amv);

    if (xQueueReceive(g_q_cmd, &c, espera) != pdTRUE)
    {
      /* Prazo estourou sem confirmacao dos detectores. */
      uint8_t falhou = 0;
      xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
      if (g_amv.estado == EST_MOVENDO)
      {
        restringe(EST_FALHA, FALHA_TIMEOUT);
        falhou = 1;
      }
      xSemaphoreGive(g_mtx_amv);
      if (falhou)
      {
        taskENTER_CRITICAL();
        g_stats.falhas++;
        taskEXIT_CRITICAL();
        amv_log(LOG_FALHA, FALHA_TIMEOUT, 0, xTaskGetTickCount() - inicio);
      }
      continue;
    }

    xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
    amv_estado_t est = g_amv.estado;

    switch (c.tipo)
    {
    case CMD_DET_OK:
      if (est == EST_MOVENDO && g_amv.detectada == g_amv.comandada)
      {
        uint32_t dur = xTaskGetTickCount() - inicio;
        g_amv.estado = EST_TRAVADO;
        g_amv.sinal  = aspecto(&g_amv);   /* tSinal vai acender no proximo ciclo */
        amv_pos_t pos = g_amv.comandada;
        xSemaphoreGive(g_mtx_amv);

        taskENTER_CRITICAL();
        g_stats.manobra_ms = dur;
        if (dur > g_stats.manobra_max_ms) g_stats.manobra_max_ms = dur;
        g_stats.manobras++;
        taskEXIT_CRITICAL();
        amv_log(LOG_TRAVADO, pos, 0, dur);
        continue;
      }
      break;

    case CMD_NORMAL:
    case CMD_REVERSA:
    case CMD_ALTERNAR:
    {
      amv_pos_t alvo = c.tipo == CMD_NORMAL  ? POS_NORMAL
                     : c.tipo == CMD_REVERSA ? POS_REVERSA
                     : (g_amv.comandada == POS_NORMAL ? POS_REVERSA : POS_NORMAL);
      int8_t rej = -1;

      if (est == EST_FALHA || est == EST_EMERGENCIA) rej = REJ_BLOQUEADO;
      else if (est == EST_MOVENDO)                   rej = REJ_EM_MOVIMENTO;
      else if (amv_hw_ocupado())                     rej = REJ_OCUPADO;
      else if (alvo == g_amv.comandada)              rej = REJ_JA_NA_POSICAO;

      if (rej < 0)
      {
        inicia_manobra(alvo, &inicio);
        xSemaphoreGive(g_mtx_amv);
        uint16_t lat = us_desde(c.t_us);
        registra_max16(&g_stats.lat_cmd_us, &g_stats.lat_cmd_max_us, lat);
        amv_log(LOG_PEDIDO, alvo, c.origem, lat);
      }
      else
      {
        xSemaphoreGive(g_mtx_amv);
        if (rej != REJ_JA_NA_POSICAO)
        {
          taskENTER_CRITICAL();
          g_stats.rejeicoes++;
          taskEXIT_CRITICAL();
        }
        amv_log(LOG_REJEITADO, (uint8_t)rej, c.origem, 0);
      }
      continue;
    }

    case CMD_REARME:
      if (est == EST_FALHA || est == EST_EMERGENCIA)
      {
        /* Rearme = refazer a manobra para a posicao comandada e so' travar
         * de novo quando os detectores confirmarem. */
        amv_pos_t alvo = g_amv.comandada;
        inicia_manobra(alvo, &inicio);
        xSemaphoreGive(g_mtx_amv);
        amv_log(LOG_REARME, 0, c.origem, 0);
        /* Ja' estava na posicao? O motor nao gera borda nova no detector,
         * entao confirma aqui mesmo. */
        if (amv_hw_detectores() == alvo)
        {
          cmd_t ok = { .tipo = CMD_DET_OK, .origem = ORIG_SISTEMA, .t_us = amv_us() };
          xQueueSend(g_q_cmd, &ok, 0);
        }
        continue;
      }
      break;
    }
    xSemaphoreGive(g_mtx_amv);
  }
}

/* ------------------------------------------------------------------ */
/* tSinal -- sinal, heartbeat, entradas lentas                          */
/* ------------------------------------------------------------------ */

static void tarefa_sinal(void *arg)
{
  (void)arg;
  TickType_t proximo = xTaskGetTickCount();
  uint8_t    ciclo = 0, rearme_ant = 0;

  for (;;)
  {
    vTaskDelayUntil(&proximo, pdMS_TO_TICKS(T_SINAL_PERIODO_MS));

    /* O aspecto e' recalculado e escrito com o mutex seguro: assim o tSeg
     * nunca e' "desfeito" por um verde calculado antes da falha. */
    xSemaphoreTake(g_mtx_amv, portMAX_DELAY);
    g_amv.ocupado   = amv_hw_ocupado();
    g_amv.obstruido = amv_hw_obstruido();
    g_amv.sinal     = aspecto(&g_amv);
    amv_hw_sinal(g_amv.sinal);
    xSemaphoreGive(g_mtx_amv);

    /* Botao de rearme (PB3), por borda de descida, amostrado a 50 ms. */
    uint8_t rearme = amv_hw_botao_rearme();
    if (rearme && !rearme_ant)
    {
      cmd_t c = { .tipo = CMD_REARME, .origem = ORIG_BOTAO, .t_us = amv_us() };
      xQueueSend(g_q_cmd, &c, 0);
    }
    rearme_ant = rearme;

    if (++ciclo >= 500 / T_SINAL_PERIODO_MS)   /* pisca LD4 a 1 Hz: CPU viva */
    {
      ciclo = 0;
      amv_hw_heartbeat();
    }
  }
}

/* ------------------------------------------------------------------ */
/* tPlanta -- SIMULACAO dos detectores de fim de curso                   */
/* ------------------------------------------------------------------ */

/* Na via real, o detector e' um contato mecanico na maquina de chave. Aqui
 * ele e' simulado: quando o servo chega ao fim de curso e a agulha nao esta'
 * obstruida, depois de T_CONTATO_MS o contato fecha. As saidas PB4/PB5 sao
 * ligadas por fio nas entradas PA0/PA1, entao o firmware de controle recebe
 * a deteccao por interrupcao, exatamente como receberia do campo. */
static void tarefa_planta(void *arg)
{
  (void)arg;
  TickType_t proximo = xTaskGetTickCount();
  amv_pos_t  saida = POS_NENHUMA, candidato = POS_NENHUMA;
  uint16_t   estavel_ms = 0;

  for (;;)
  {
    vTaskDelayUntil(&proximo, pdMS_TO_TICKS(T_PLANTA_PERIODO_MS));

    uint16_t  ang = g_angulo_cdeg;
    amv_pos_t pos = ang == ANG_NORMAL_CDEG  ? POS_NORMAL
                  : ang == ANG_REVERSA_CDEG ? POS_REVERSA : POS_NENHUMA;
    if (amv_hw_obstruido()) pos = POS_NENHUMA;

    if (pos != candidato) { candidato = pos; estavel_ms = 0; }
    else if (estavel_ms < T_CONTATO_MS) estavel_ms += T_PLANTA_PERIODO_MS;

    /* Abrir o contato e' imediato; fechar exige T_CONTATO_MS estavel. */
    amv_pos_t nova = (candidato == POS_NENHUMA || estavel_ms >= T_CONTATO_MS)
                   ? candidato : POS_NENHUMA;
    if (nova != saida)
    {
      saida = nova;
      amv_hw_planta_detector(saida);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Ganchos do FreeRTOS                                                 */
/* ------------------------------------------------------------------ */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask; (void)pcTaskName;
  amv_hw_estado_seguro();
  __disable_irq();
  for (;;) {}
}

void amv_assert_failed(const char *file, int line)
{
  (void)file; (void)line;
  amv_hw_estado_seguro();
  __disable_irq();
  for (;;) {}
}

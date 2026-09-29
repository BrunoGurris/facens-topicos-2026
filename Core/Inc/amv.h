/*
 * amv.h -- Controlador de AMV (Aparelho de Mudanca de Via / desvio ferroviario)
 *
 * Modelo simplificado de um AMV com maquina de chave eletrica:
 *   - maquina de chave (servo)  : move as agulhas entre NORMAL e REVERSA
 *   - detectores de fim de curso: confirmam que a agulha encostou no trilho
 *   - circuito de via (ocupacao): trem sobre o AMV -> proibido mover
 *   - sinal de protecao         : vermelho / amarelo (desviada) / verde (reta)
 *   - botao de emergencia       : leva tudo ao estado seguro
 *
 * Regra de seguranca central: o sinal so' sai do vermelho com o AMV TRAVADO,
 * isto e', posicao comandada == posicao detectada e sem falha/emergencia.
 */
#ifndef AMV_H
#define AMV_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ======================================================================
 * Parametros de tempo (ver docs/ARQUITETURA.md para os calculos)
 * ====================================================================== */
#define T_MOTOR_PERIODO_MS     20      /* passo do motor = periodo do PWM do servo */
#define T_MANOBRA_MS           3000    /* tempo nominal NORMAL <-> REVERSA          */
#define T_WATCHDOG_MS          4500    /* limite p/ confirmar deteccao (1,5 x nominal) */
#define T_CONTATO_MS           100     /* atraso do contato do detector (planta)    */
#define T_PLANTA_PERIODO_MS    10
#define T_SINAL_PERIODO_MS     50
#define T_STATUS_PERIODO_MS    250
#define T_DEBOUNCE_MS          200

/* Angulos da maquina de chave, em centesimos de grau (evita float no M0+) */
#define ANG_NORMAL_CDEG        4500    /* 45 graus  */
#define ANG_REVERSA_CDEG       13500   /* 135 graus */
#define ANG_INICIAL_CDEG       9000    /* 90 graus: posicao indefinida no boot */
#define ANG_PASSO_CDEG  ((ANG_REVERSA_CDEG - ANG_NORMAL_CDEG) / (T_MANOBRA_MS / T_MOTOR_PERIODO_MS))

/* Prioridades (maior numero = mais prioritaria) */
#define PRIO_SEGURANCA         5
#define PRIO_MOTOR             4
#define PRIO_INTERTRAV         3
#define PRIO_SINAL             2
#define PRIO_PLANTA            2
#define PRIO_COMM              1

/* ======================================================================
 * Tipos
 * ====================================================================== */
typedef enum { POS_NENHUMA = 0, POS_NORMAL, POS_REVERSA, POS_DUPLA } amv_pos_t;

typedef enum {
  EST_MOVENDO = 0,   /* motor girando, sinal vermelho, watchdog armado  */
  EST_TRAVADO,       /* posicao confirmada: unico estado com via livre  */
  EST_FALHA,         /* timeout ou perda de deteccao: exige rearme      */
  EST_EMERGENCIA,    /* botao de emergencia: exige rearme               */
} amv_estado_t;

typedef enum { SINAL_VERMELHO = 0, SINAL_AMARELO, SINAL_VERDE } amv_sinal_t;

typedef enum {
  FALHA_NENHUMA = 0,
  FALHA_TIMEOUT,        /* detector nao confirmou dentro do watchdog   */
  FALHA_PERDA_DET,      /* detector caiu com o AMV travado             */
  FALHA_DET_DUPLA,      /* os dois detectores ativos: sensor com defeito */
} amv_falha_t;

/* Estado compartilhado do AMV -- protegido por g_mtx_amv */
typedef struct {
  amv_estado_t estado;
  amv_pos_t    comandada;
  amv_pos_t    detectada;
  amv_sinal_t  sinal;
  amv_falha_t  falha;
  uint8_t      ocupado;       /* circuito de via */
  uint8_t      obstruido;     /* injecao de falha (simulacao) */
} amv_status_t;

/* Eventos para a tarefa de seguranca (fila g_q_seg) -- vem de ISR */
typedef enum { SEG_EMERGENCIA = 0, SEG_DETECTOR } seg_tipo_t;
typedef struct {
  uint8_t  tipo;     /* seg_tipo_t */
  uint16_t t_us;     /* carimbo TIM14 no momento da interrupcao */
} seg_evt_t;

/* Comandos para o intertravamento (fila g_q_cmd) */
typedef enum { CMD_NORMAL = 0, CMD_REVERSA, CMD_ALTERNAR, CMD_REARME, CMD_DET_OK } cmd_tipo_t;
typedef enum { ORIG_SERIAL = 0, ORIG_BOTAO, ORIG_SISTEMA } cmd_origem_t;
typedef struct {
  uint8_t  tipo;     /* cmd_tipo_t   */
  uint8_t  origem;   /* cmd_origem_t */
  uint16_t t_us;     /* carimbo de quando o pedido nasceu */
} cmd_t;

/* Comandos para a maquina de chave (fila g_q_motor) */
typedef enum { MOTOR_MOVER = 0, MOTOR_PARAR } motor_tipo_t;
typedef struct {
  uint8_t  tipo;
  uint16_t alvo_cdeg;
} motor_cmd_t;

/* Mensagens para a tarefa de comunicacao (fila g_q_log) */
typedef enum {
  LOG_PEDIDO = 0,     /* a=posicao alvo, b=origem, v=latencia pedido->motor (us) */
  LOG_REJEITADO,      /* a=motivo, b=origem                                      */
  LOG_TRAVADO,        /* a=posicao, v=duracao da manobra (ms)                    */
  LOG_FALHA,          /* a=amv_falha_t, v=latencia ate sinal vermelho (us)       */
  LOG_EMERGENCIA,     /* v=latencia botao -> sinal vermelho (us)                 */
  LOG_REARME,
} log_tipo_t;
typedef enum { REJ_OCUPADO = 0, REJ_EM_MOVIMENTO, REJ_BLOQUEADO, REJ_JA_NA_POSICAO } rej_motivo_t;
typedef struct {
  uint8_t  tipo;
  uint8_t  a;
  uint8_t  b;
  uint32_t t_ms;
  uint32_t v;
} log_msg_t;

/* Estatisticas de tempo (medido x calculado) -- secao critica curta */
typedef struct {
  uint16_t lat_emerg_us,  lat_emerg_max_us;    /* ISR botao   -> sinal vermelho */
  uint16_t lat_perda_us,  lat_perda_max_us;    /* ISR detector-> sinal vermelho */
  uint16_t lat_cmd_us,    lat_cmd_max_us;      /* pedido      -> motor comandado */
  uint32_t manobra_ms,    manobra_max_ms;      /* motor liga  -> AMV travado     */
  uint16_t jitter_motor_max_us;                /* desvio do periodo de 20 ms     */
  uint32_t manobras, falhas, rejeicoes;
} amv_stats_t;

/* ======================================================================
 * Objetos do RTOS (criados em amv_tasks.c)
 * ====================================================================== */
extern QueueHandle_t     g_q_seg;
extern QueueHandle_t     g_q_cmd;
extern QueueHandle_t     g_q_motor;
extern QueueHandle_t     g_q_log;
extern QueueHandle_t     g_q_rx;
extern SemaphoreHandle_t g_mtx_amv;
extern SemaphoreHandle_t g_sem_tx;

extern amv_status_t      g_amv;       /* so' com g_mtx_amv */
extern amv_stats_t       g_stats;     /* so' com taskENTER_CRITICAL */
extern volatile uint16_t g_angulo_cdeg;  /* escrito so' pelo tMotor (16 bits = atomico) */

/* ======================================================================
 * Funcoes
 * ====================================================================== */
/* amv_hw.c -- hardware */
void        amv_hw_init(void);
uint16_t    amv_us(void);                      /* contador livre de 1 us (TIM14) */
void        amv_hw_servo(uint16_t cdeg);
void        amv_hw_sinal(amv_sinal_t s);
void        amv_hw_heartbeat(void);
amv_pos_t   amv_hw_detectores(void);
uint8_t     amv_hw_ocupado(void);
uint8_t     amv_hw_obstruido(void);
uint8_t     amv_hw_botao_rearme(void);
void        amv_hw_planta_detector(amv_pos_t p); /* saidas simuladas dos detectores */
void        amv_hw_estado_seguro(void);
void        amv_hw_uart_rx_start(void);

/* amv_tasks.c */
void        amv_start(void);                   /* cria tudo e inicia o escalonador */
void        amv_log(uint8_t tipo, uint8_t a, uint8_t b, uint32_t v);
void        amv_status_copia(amv_status_t *dst);
uint16_t    amv_stack_livre(uint8_t idx, const char **nome);

/* amv_comm.c */
void        tarefa_comm(void *arg);
extern volatile uint8_t g_ocupado_virtual;    /* comandos "occ"/"obs" da serial */
extern volatile uint8_t g_obstruido_virtual;

#endif /* AMV_H */

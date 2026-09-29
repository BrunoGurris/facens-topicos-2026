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
#define T_PENDENTE_POLL_MS     50      /* reavalia o pedido em espera          */

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
#define PRIO_TREM              2
#define PRIO_COMM              1

/* ======================================================================
 * Trem simulado (tarefa tTrem, amv_trem.c) -- unidades: mm e mm/s
 * Comprimentos na mesma escala do desenho do painel (1 px ~ 0,32 m).
 *
 *  circuito fechado:  ... VOLTA --[sinal]--> ZONA (AMV) --> ROTA N ou R --> VOLTA ...
 *  a rota R e' um ramal paralelo que volta para a linha principal (juncao de mola)
 * ====================================================================== */
#define T_TREM_PERIODO_MS      50
#define TREM_L_VOLTA_MM        335000u  /* linha comum (da juncao ate' o AMV)    */
#define TREM_L_ZONA_MM         16000u   /* aparelho de mudanca de via            */
#define TREM_L_ROTA_N_MM       86000u   /* rota normal (reta)                    */
#define TREM_L_ROTA_R_MM       97000u   /* rota reversa (ramal desviado)         */
#define TREM_S_SINAL_MM        (TREM_L_VOLTA_MM - 40000u)  /* sinal 40 m antes do AMV */
#define TREM_COMPRIMENTO_MM    30000u
#define TREM_MARGEM_MM         5000u    /* para 5 m antes do sinal vermelho      */
#define TREM_A_ACEL            800u     /* mm/s^2 */
#define TREM_A_SERVICO         1200u    /* frenagem normal     */
#define TREM_A_EMERG           2500u    /* frenagem de emergencia */
#define TREM_V_DESVIO_KMH      30u      /* limite na via desviada (sinal amarelo) */
#define TREM_V_MAX_KMH         80u      /* acima disso a frenagem nao cabe na volta */
#define TREM_V_INICIAL_KMH     40u

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
  amv_pos_t    pendente;      /* pedido em espera (POS_NENHUMA = nenhum) */
} amv_status_t;

/* Eventos para a tarefa de seguranca (fila g_q_seg) -- vem de ISR */
typedef enum { SEG_EMERGENCIA = 0, SEG_DETECTOR } seg_tipo_t;
typedef struct {
  uint8_t  tipo;     /* seg_tipo_t */
  uint16_t t_us;     /* carimbo TIM14 no momento da interrupcao */
} seg_evt_t;

/* Comandos para o intertravamento (fila g_q_cmd) */
typedef enum { CMD_NORMAL = 0, CMD_REVERSA, CMD_ALTERNAR, CMD_REARME, CMD_DET_OK } cmd_tipo_t;
typedef enum { ORIG_SERIAL = 0, ORIG_BOTAO, ORIG_SISTEMA, ORIG_FILA } cmd_origem_t;
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
  LOG_TRAVADO,        /* a=posicao, b=1 se confirmado por nivel, v=duracao (ms)  */
  LOG_FALHA,          /* a=amv_falha_t, v=latencia ate sinal vermelho (us);      */
                      /* timeout: b=diagnostico (DIAG_*), v=ms desde o inicio    */
  LOG_EMERGENCIA,     /* v=latencia botao -> sinal vermelho (us)                 */
  LOG_REARME,
  LOG_TREM,           /* a=trem_evento_t, b=rota, v=velocidade (km/h)            */
  LOG_PENDENTE,       /* a=alvo, b=motivo; cancelado: b=REJ_BLOQUEADO (falha)    */
                      /* ou REJ_JA_NA_POSICAO (pedido novo p/ posicao atual)     */
} log_tipo_t;
typedef enum { REJ_OCUPADO = 0, REJ_EM_MOVIMENTO, REJ_BLOQUEADO, REJ_JA_NA_POSICAO } rej_motivo_t;
typedef struct {
  uint8_t  tipo;
  uint8_t  a;
  uint8_t  b;
  uint32_t t_ms;
  uint32_t v;
} log_msg_t;

/* Trem simulado -- escrito so' pelo tTrem, lido com taskENTER_CRITICAL */
typedef enum { TSEG_VOLTA = 0, TSEG_ZONA, TSEG_ROTA } trem_seg_t;
typedef enum { FREIO_NENHUM = 0, FREIO_SERVICO, FREIO_EMERGENCIA } trem_freio_t;
typedef enum { TREM_ENTROU = 0, TREM_PASSOU_VERMELHO, TREM_DESCARRILOU, TREM_RECOLOCADO } trem_evento_t;
typedef struct {
  uint8_t  seg;          /* trem_seg_t: onde esta' a frente do trem          */
  uint8_t  rota;         /* amv_pos_t: rota tomada na ultima passagem        */
  uint8_t  freio;        /* trem_freio_t                                     */
  uint8_t  ocupado;      /* circuito de via / aproximacao travada            */
  uint8_t  descarrilado;
  uint32_t s_mm;         /* posicao da frente dentro do segmento             */
  uint32_t v_mms;        /* velocidade atual                                 */
} trem_t;

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
extern volatile uint8_t  g_planta_saida; /* amv_pos_t que a tPlanta esta' mandando nos fios */

/* Diagnostico do timeout (campo b do LOG_FALHA): o que se via no instante */
#define DIAG_PINOS(b)   ((b) & 0x3)          /* amv_pos_t lido em PA0/PA1   */
#define DIAG_PLANTA(b)  (((b) >> 2) & 0x3)   /* amv_pos_t na saida PB4/PB5  */
#define DIAG_OBS(b)     (((b) >> 4) & 0x1)   /* obstrucao ativa             */
#define DIAG(pinos, planta, obs) ((uint8_t)((pinos) | ((planta) << 2) | ((obs) << 4)))

#define T_CONFIRMA_POLL_MS     50      /* leitura por nivel dos detectores na manobra */

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
void        amv_trem_copia(trem_t *dst);
extern volatile uint8_t g_trem_vel_kmh;       /* comando "vel <km/h>"          */
extern volatile uint8_t g_trem_recolocar;     /* comando "trem"                */
extern volatile uint8_t g_trem_ocupado;       /* entra em amv_hw_ocupado()     */

/* amv_trem.c */
void        tarefa_trem(void *arg);
extern volatile uint8_t g_ocupado_virtual;    /* comandos "occ"/"obs" da serial */
extern volatile uint8_t g_obstruido_virtual;

#endif /* AMV_H */

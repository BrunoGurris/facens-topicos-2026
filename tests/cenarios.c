/*
 * cenarios.c -- roteiros de teste do controlador de AMV rodando no PC
 *
 * As tarefas de Core/Src/amv_tasks.c, amv_trem.c e amv_comm.c rodam sem
 * alteracao, sobre o kernel FreeRTOS real (port POSIX). Esta tarefa extra
 * faz o papel do operador: escreve comandos na fila g_q_rx como se fossem
 * bytes chegando pela USART2. O que a placa responde sai no stdout, e o
 * tests/run.sh confere as linhas esperadas.
 *
 * Uso: ./hil <cenario>     (tests/run.sh roda todos)
 */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "amv.h"

extern volatile uint8_t hil_fio_r_quebrado;

static void cmd(const char *s)
{
  printf(">>> [%lu ms] %s\n", (unsigned long)xTaskGetTickCount(), s);
  fflush(stdout);
  for (; *s; s++)
    xQueueSend(g_q_rx, s, portMAX_DELAY);
  uint8_t nl = '\n';
  xQueueSend(g_q_rx, &nl, portMAX_DELAY);
}

static void espera(unsigned ms)
{
  vTaskDelay(pdMS_TO_TICKS(ms));
}

/* Manobras alternadas com o trem circulando a 40 km/h (ocupacao intermitente). */
static void cen_manobras(void)
{
  for (int i = 0; i < 8; i++) { cmd(i % 2 ? "n" : "r"); espera(9000); }
}

/* Trem a 80 km/h: circuito ocupado ~2/3 do tempo, pedidos a cada 4 s. */
static void cen_trem80(void)
{
  cmd("vel 80"); espera(3000);
  for (int i = 0; i < 10; i++) { cmd(i % 2 ? "n" : "r"); espera(4000); }
}

/* Sem trem: varios pedidos em cima de uma manobra em curso. */
static void cen_cliques(void)
{
  cmd("vel 0"); espera(1000);
  cmd("r"); espera(500); cmd("n"); espera(500); cmd("r"); espera(6000);
  cmd("t"); espera(300); cmd("t"); espera(300); cmd("t"); espera(8000);
  cmd("r"); espera(5000); cmd("n"); espera(5000);
}

/* Emergencia no meio da manobra, rearme, e manobras normais depois. */
static void cen_emergencia(void)
{
  cmd("vel 0"); espera(500);
  cmd("r"); espera(1500);
  cmd("emg"); espera(1000);
  cmd("rearme"); espera(5000);
  cmd("n"); espera(5000);
  cmd("r"); espera(5000);
}

/* Pedido em espera: circuito ocupado guarda o pedido; liberou, executa. */
static void cen_fila(void)
{
  cmd("vel 0"); espera(500);
  cmd("occ 1"); espera(300);
  cmd("r"); espera(3000);          /* nada pode se mover aqui */
  cmd("occ 0"); espera(5000);      /* agora sai sozinho */
}

/* Obstrucao com o AMV travado -> perda de deteccao; rearme com obstrucao ->
 * timeout; tira a obstrucao e rearma -> trava. */
static void cen_obstrucao(void)
{
  cmd("vel 0"); espera(500);
  cmd("obs 1"); espera(500);
  cmd("rearme"); espera(5500);
  cmd("obs 0"); espera(300);
  cmd("rearme"); espera(2000);
}

/* Controle: fio PB5 -> PA1 rompido. Tem que dar timeout com diagnostico. */
static void cen_fio_quebrado(void)
{
  cmd("vel 0"); hil_fio_r_quebrado = 1; espera(500);
  cmd("r"); espera(6000);
}

static const struct { const char *nome; void (*fn)(void); } CENARIOS[] = {
  { "manobras",     cen_manobras },
  { "trem80",       cen_trem80 },
  { "cliques",      cen_cliques },
  { "emergencia",   cen_emergencia },
  { "fila",         cen_fila },
  { "obstrucao",    cen_obstrucao },
  { "fio_quebrado", cen_fio_quebrado },
};

static const char *escolhido;

static void tarefa_operador(void *arg)
{
  (void)arg;
  espera(4000);                    /* deixa o boot travar em Normal */
  for (unsigned i = 0; i < sizeof(CENARIOS) / sizeof(CENARIOS[0]); i++)
    if (!strcmp(escolhido, CENARIOS[i].nome))
      CENARIOS[i].fn();
  cmd("stats");
  espera(300);
  printf("FIM\n");
  fflush(stdout);
  exit(0);
}

int main(int argc, char **argv)
{
  static StackType_t  stk[8192];
  static StaticTask_t tcb;

  if (argc < 2)
  {
    fprintf(stderr, "uso: %s <cenario>\ncenarios:", argv[0]);
    for (unsigned i = 0; i < sizeof(CENARIOS) / sizeof(CENARIOS[0]); i++)
      fprintf(stderr, " %s", CENARIOS[i].nome);
    fprintf(stderr, "\n");
    return 1;
  }
  escolhido = argv[1];
  setvbuf(stdout, NULL, _IOLBF, 0);
  xTaskCreateStatic(tarefa_operador, "tOper", 8192, NULL, 1, stk, &tcb);
  amv_start();                     /* cria as tarefas do AMV e inicia o escalonador */
  return 1;
}

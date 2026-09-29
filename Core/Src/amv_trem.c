/*
 * amv_trem.c -- SIMULACAO de um trem circulando pelo AMV (tarefa tTrem)
 *
 * Assim como a tPlanta, esta tarefa nao existiria no controlador real: ela
 * faz o papel do mundo fisico para exercitar o intertravamento.
 *
 * Circuito fechado (posicoes em mm, velocidades em mm/s, inteiros):
 *
 *   VOLTA (335 m) --[sinal a 40 m do AMV]--> ZONA (16 m) --> ROTA N (86 m) --+
 *     ^                                                  \-> ROTA R (97 m) --+
 *     +--------------------------- juncao de mola <------------------------------+
 *
 * O trem:
 *  - obedece o sinal com uma curva de frenagem: v_perm = sqrt(v_final^2 + 2*a*d)
 *    (vermelho: parar 5 m antes do sinal; amarelo: chegar a 30 km/h);
 *  - se o sinal fecha quando ja' nao da' para parar com frenagem de servico,
 *    aplica frenagem de EMERGENCIA e pode passar o sinal vermelho;
 *  - ao entrar no AMV, segue a rota que os detectores indicam; se a agulha
 *    nao estiver encostada em nenhum lado, DESCARRILA;
 *  - ocupa o circuito de via do sinal ate' a cauda sair do AMV, e tambem
 *    trava a aproximacao quando ja' nao consegue parar antes do sinal.
 */
#include "main.h"
#include "amv.h"

volatile uint8_t g_trem_vel_kmh = TREM_V_INICIAL_KMH;
volatile uint8_t g_trem_recolocar;
volatile uint8_t g_trem_ocupado;

static trem_t g_trem = { .seg = TSEG_VOLTA, .rota = POS_NORMAL };

void amv_trem_copia(trem_t *dst)
{
  taskENTER_CRITICAL();
  *dst = g_trem;
  taskEXIT_CRITICAL();
}

static uint32_t isqrt(uint32_t x)
{
  uint32_t r = 0, b = 1u << 30;
  while (b > x) b >>= 2;
  while (b)
  {
    if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
    else            { r >>= 1; }
    b >>= 2;
  }
  return r;
}

static uint32_t kmh_para_mms(uint32_t kmh)
{
  return kmh * 10000u / 36u;
}

/* Maior velocidade que ainda permite chegar a v_final em 'dist' mm. */
static uint32_t v_permitida(uint32_t dist, uint32_t v_final, uint32_t a)
{
  return isqrt(v_final * v_final + 2u * a * dist);
}

static uint32_t comprimento_rota(uint8_t rota)
{
  return rota == POS_REVERSA ? TREM_L_ROTA_R_MM : TREM_L_ROTA_N_MM;
}

void tarefa_trem(void *arg)
{
  (void)arg;
  TickType_t   proximo = xTaskGetTickCount();
  trem_t       t = g_trem;
  amv_status_t amv;
  uint8_t      restrito = 0;   /* passou o sinal em amarelo: 30 km/h ate' sair do ramal */
  uint8_t      retido = 0;     /* parou em emergencia: so' anda quando o sinal abrir */

  for (;;)
  {
    vTaskDelayUntil(&proximo, pdMS_TO_TICKS(T_TREM_PERIODO_MS));
    amv_status_copia(&amv);

    if (g_trem_recolocar)
    {
      g_trem_recolocar = 0;
      t = (trem_t){ .seg = TSEG_VOLTA, .rota = POS_NORMAL };
      restrito = retido = 0;
      amv_log(LOG_TREM, TREM_RECOLOCADO, 0, 0);
    }

    /* ---- velocidade maxima permitida neste instante ---- */
    uint32_t v_lim = kmh_para_mms(g_trem_vel_kmh);
    uint8_t  emerg = 0;
    uint32_t v_desvio = kmh_para_mms(TREM_V_DESVIO_KMH);

    if (t.descarrilado)
      v_lim = 0;
    if ((restrito || (t.seg != TSEG_VOLTA && t.rota == POS_REVERSA)) && v_lim > v_desvio)
      v_lim = v_desvio;
    /* Frenagem de emergencia, uma vez aplicada, so' solta com o trem parado. */
    if (t.freio == FREIO_EMERGENCIA && t.v_mms > 0)
      emerg = 1;
    if (t.freio == FREIO_EMERGENCIA && t.v_mms == 0)
      retido = 1;
    if (retido && amv.sinal != SINAL_VERMELHO)
      retido = 0;
    if (retido)
      v_lim = 0;

    uint32_t livre = 0;            /* distancia util ate' o ponto de parada */
    uint8_t  antes_do_sinal = t.seg == TSEG_VOLTA && t.s_mm < TREM_S_SINAL_MM;
    if (antes_do_sinal)
    {
      /* Olha um passo a frente: o trem ainda vai andar v*dt antes da
       * proxima decisao, senao comeca a frear sempre um pouco atrasado. */
      uint32_t passo = t.v_mms * T_TREM_PERIODO_MS / 1000u;
      uint32_t dist  = TREM_S_SINAL_MM - t.s_mm;
      dist  = dist > passo ? dist - passo : 0;
      livre = dist > TREM_MARGEM_MM ? dist - TREM_MARGEM_MM : 0;

      if (amv.sinal == SINAL_VERMELHO)
      {
        uint32_t vp = v_permitida(livre, 0, TREM_A_SERVICO);
        if (vp < v_lim) v_lim = vp;
        /* Precisa de mais que 110% da frenagem de servico (com tolerancia de
         * 0,5 m/s, para o arredondamento perto da parada)? -> emergencia */
        if ((uint64_t)t.v_mms * t.v_mms
            > (uint64_t)2u * TREM_A_SERVICO * 11u / 10u * livre + 500u * 500u)
          emerg = 1;
      }
      else if (amv.sinal == SINAL_AMARELO)
      {
        uint32_t vp = v_permitida(dist, v_desvio, TREM_A_SERVICO);
        if (vp < v_lim) v_lim = vp;
      }
    }

    /* ---- dinamica (passo de 50 ms) ---- */
    uint32_t dv_acel = TREM_A_ACEL    * T_TREM_PERIODO_MS / 1000u;
    uint32_t dv_serv = TREM_A_SERVICO * T_TREM_PERIODO_MS / 1000u;
    uint32_t dv_emg  = TREM_A_EMERG   * T_TREM_PERIODO_MS / 1000u;

    if (emerg)
    {
      t.freio = FREIO_EMERGENCIA;
      t.v_mms = t.v_mms > dv_emg ? t.v_mms - dv_emg : 0;
    }
    else if (t.v_mms > v_lim)
    {
      t.freio = FREIO_SERVICO;
      t.v_mms = t.v_mms > v_lim + dv_serv ? t.v_mms - dv_serv : v_lim;
    }
    else
    {
      t.freio = FREIO_NENHUM;
      t.v_mms = t.v_mms + dv_acel < v_lim ? t.v_mms + dv_acel : v_lim;
    }

    uint32_t s_ant = t.s_mm;
    t.s_mm += t.v_mms * T_TREM_PERIODO_MS / 1000u;

    /* ---- eventos e troca de segmento ---- */
    if (t.seg == TSEG_VOLTA && s_ant < TREM_S_SINAL_MM && t.s_mm >= TREM_S_SINAL_MM
)
    {
      if (amv.sinal == SINAL_VERMELHO)
        amv_log(LOG_TREM, TREM_PASSOU_VERMELHO, 0, t.v_mms * 36u / 10000u);
      restrito = amv.sinal == SINAL_AMARELO;
    }

    if (t.seg == TSEG_VOLTA && t.s_mm >= TREM_L_VOLTA_MM)
    {
      t.s_mm -= TREM_L_VOLTA_MM;
      t.seg = TSEG_ZONA;
      if (amv.detectada == POS_NORMAL || amv.detectada == POS_REVERSA)
      {
        t.rota = amv.detectada;
        amv_log(LOG_TREM, TREM_ENTROU, t.rota, t.v_mms * 36u / 10000u);
      }
      else
      {
        /* agulha no meio do curso ou obstruida: o trem sai dos trilhos */
        t.descarrilado = 1;
        t.v_mms = 0;
        amv_log(LOG_TREM, TREM_DESCARRILOU, 0, 0);
      }
    }
    else if (t.seg == TSEG_ZONA && t.s_mm >= TREM_L_ZONA_MM)
    {
      t.s_mm -= TREM_L_ZONA_MM;
      t.seg = TSEG_ROTA;
    }
    else if (t.seg == TSEG_ROTA && t.s_mm >= comprimento_rota(t.rota))
    {
      t.s_mm -= comprimento_rota(t.rota);
      t.seg = TSEG_VOLTA;
      restrito = 0;
    }

    /* ---- circuito de via ----
     * Ocupado do sinal ate' a cauda sair do AMV, ou antes do sinal se o trem
     * ja' nao consegue parar nele (travamento de aproximacao): com sinal
     * aberto, quando esta' dentro da distancia de frenagem; com sinal
     * fechado, enquanto estiver em frenagem de emergencia. */
    uint8_t comprometido = antes_do_sinal
        && ((amv.sinal != SINAL_VERMELHO
             && (uint64_t)t.v_mms * t.v_mms > (uint64_t)2u * TREM_A_SERVICO * livre)
            || t.freio == FREIO_EMERGENCIA);
    t.ocupado = comprometido
             || (t.seg == TSEG_VOLTA && t.s_mm >= TREM_S_SINAL_MM)
             || t.seg == TSEG_ZONA
             || (t.seg == TSEG_ROTA && t.s_mm < TREM_COMPRIMENTO_MM)
             || t.descarrilado;
    g_trem_ocupado = t.ocupado;

    taskENTER_CRITICAL();
    g_trem = t;
    taskEXIT_CRITICAL();
  }
}

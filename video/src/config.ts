/*
 * A gravação (public/gravacao.mp4) e as marcas (public/marcas.json) são
 * geradas por `node gravar.mjs`, que comanda o Wokwi pelo painel e grava a tela.
 * Cada trecho começa em marcas[marca] + apos (segundos da gravação).
 *
 * duracao   = segundos no vídeo final
 * velocidade = aceleração do trecho (consome duracao × velocidade s da gravação).
 * A simulação do Wokwi roda a ~0,7× do tempo real, por isso os trechos longos
 * (trem andando) são acelerados.
 */
import dados from "../public/marcas.json";

export const FPS = 30;

export const GRAVACAO: string | null = "gravacao.mp4"; // null = mostra o que gravar no lugar

export const MARCAS: Record<string, number> = dados.marcas;
/* Tempos mostrados no vídeo (legendas e cena "Medido × meta"): os da Tabela 14
 * do relatório (docs/secao_5.md). Para usar os da execução gravada, troque por
 * `export const TM = dados.tm;` */
export const TM = {
  lat_emg: [200, 200],
  lat_perda: [253, 253],
  lat_cmd: [368, 368],
  manobra: [3110, 3110],
  jit: 556,
};

export type Trecho = {
  id: string;
  marca: string;
  apos: number;
  duracao: number;
  velocidade?: number;
  legenda: string;
  oQueGravar: string;
};

export const TRECHOS: Trecho[] = [
  {
    id: "boot",
    marca: "boot",
    apos: -0.2,
    duracao: 5,
    legenda: "Boot: a agulha vai para Normal e trava em 1,6 s → sinal verde",
    oQueGravar: "Reiniciar tudo → servo vai a 45°, sinal verde",
  },
  {
    id: "manobra",
    marca: "manobra",
    apos: 0,
    duracao: 6,
    velocidade: 1.1,
    legenda: "Pedido de Reversa: o sinal fecha na hora · manobra de 3,1 s",
    oQueGravar: "Botão Reversa → vermelho, agulha move, amarelo",
  },
  {
    id: "ramal",
    marca: "ramal",
    apos: 0,
    duracao: 8.5,
    velocidade: 2.5,
    legenda: "Sinal amarelo: o trem reduz de 60 para 30 km/h e entra no ramal",
    oQueGravar: "Trem a 60 km/h se aproximando com o AMV em Reversa",
  },
  {
    id: "espera",
    marca: "espera",
    apos: -0.5,
    duracao: 5,
    velocidade: 1.6,
    legenda: "Trem sobre o AMV: o pedido espera e sai sozinho quando a via libera",
    oQueGravar: "Pedido de Normal com o trem no desvio",
  },
  {
    id: "emerg",
    marca: "emerg",
    apos: -0.5,
    duracao: 8.5,
    velocidade: 2.7,
    legenda: `Emergência: sinal vermelho em ${TM.lat_emg[1]} µs → o trem freia e para antes do sinal`,
    oQueGravar: "Emergência com o trem a ~140 m do sinal",
  },
  {
    id: "obstr",
    marca: "obstr",
    apos: -0.3,
    duracao: 4.5,
    legenda: `Agulha obstruída: perda de detecção → vermelho em ${TM.lat_perda[1]} µs`,
    oQueGravar: "AMV travado → obstruir → FALHA",
  },
];

export const inicioDe = (t: Trecho) => Math.max(0, (MARCAS[t.marca] ?? 0) + t.apos);

/* Duração (s) das cenas geradas. */
export const D_TITULO = 4;
export const D_FRENAGEM = 9;
export const D_NUMEROS = 7;
export const D_FIM = 3.5;
export const TRANSICAO = 10; // quadros de fade entre cenas

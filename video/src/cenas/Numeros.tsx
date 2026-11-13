import React from "react";
import { interpolate, spring, useCurrentFrame, useVideoConfig, Easing } from "remotion";
import { TM } from "../config";
import { C, Fundo } from "../estilo";

/* Tempos da MESMA execução gravada (public/marcas.json, linha "tm" da placa). */
type Linha = { nome: string; medido: number; meta: number; fmt: (x: number) => string; metaTxt: string };
const us = (x: number) => `${Math.round(x)} µs`;
const LINHAS: Linha[] = [
  { nome: "Emergência → sinal vermelho", medido: TM.lat_emg[1], meta: 10000, fmt: us, metaTxt: "≤ 10 ms" },
  { nome: "Perda de detecção → vermelho", medido: TM.lat_perda[1], meta: 10000, fmt: us, metaTxt: "≤ 10 ms" },
  { nome: "Pedido → motor comandado", medido: TM.lat_cmd[1], meta: 50000, fmt: us, metaTxt: "≤ 50 ms" },
  { nome: "Manobra completa", medido: TM.manobra[1] / 1000, meta: 4.5, fmt: (x) => `${x.toFixed(1).replace(".", ",")} s`, metaTxt: "≤ 4,5 s" },
  { nome: "Jitter do motor", medido: TM.jit, meta: 1000, fmt: us, metaTxt: "≤ 1 ms" },
];

const BARRA = 520;

export const Numeros: React.FC = () => {
  const f = useCurrentFrame();
  const { fps } = useVideoConfig();

  return (
    <Fundo>
      <div style={{ position: "absolute", left: 150, top: 70 }}>
        <div style={{ fontSize: 72, fontWeight: 800, letterSpacing: -1 }}>Medido na placa × meta</div>
        <div style={{ fontSize: 34, color: C.mut, marginTop: 8 }}>cronômetro de 1 µs (TIM14) no próprio STM32</div>
      </div>

      <div style={{ position: "absolute", left: 150, right: 150, top: 290, display: "flex", flexDirection: "column", gap: 40 }}>
        {LINHAS.map((l, i) => {
          const ini = 8 + i * 10;
          const s = spring({ frame: f - ini, fps, config: { damping: 200 } });
          const p = interpolate(f, [ini, ini + 35], [0, 1], {
            extrapolateLeft: "clamp",
            extrapolateRight: "clamp",
            easing: Easing.out(Easing.cubic),
          });
          const frac = l.medido / l.meta;
          const ok = spring({ frame: f - ini - 35, fps, config: { damping: 12 } });
          return (
            <div key={l.nome} style={{ display: "flex", alignItems: "center", opacity: s, transform: `translateY(${(1 - s) * 20}px)` }}>
              <div style={{ width: 640, fontSize: 42 }}>{l.nome}</div>
              <div style={{ width: 230, fontSize: 46, fontWeight: 800, color: C.acc, textAlign: "right" }}>{l.fmt(l.medido * p)}</div>
              <div style={{ marginLeft: 50, width: BARRA }}>
                <div style={{ height: 22, borderRadius: 11, background: C.linha, position: "relative", overflow: "hidden" }}>
                  <div
                    style={{
                      position: "absolute",
                      inset: 0,
                      width: Math.max(10, BARRA * frac * p),
                      borderRadius: 11,
                      background: C.acc,
                    }}
                  />
                </div>
                <div style={{ display: "flex", justifyContent: "space-between", fontSize: 24, color: C.mut, marginTop: 6 }}>
                  <span>{(frac * 100 * p).toFixed(frac < 0.1 ? 1 : 0).replace(".", ",")} % da meta</span>
                  <span>meta {l.metaTxt}</span>
                </div>
              </div>
              <div style={{ marginLeft: 40, fontSize: 52, fontWeight: 800, color: C.verde, transform: `scale(${ok})` }}>✓</div>
            </div>
          );
        })}
      </div>

      <div
        style={{
          position: "absolute",
          left: 150,
          bottom: 80,
          fontSize: 32,
          color: C.mut,
          opacity: spring({ frame: f - 90, fps, config: { damping: 200 } }),
        }}
      >
        7 tarefas · 5 filas · 1 mutex com herança de prioridade · 7/7 cenários de teste automático aprovados
      </div>
    </Fundo>
  );
};

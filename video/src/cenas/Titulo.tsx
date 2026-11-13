import React from "react";
import { interpolate, spring, useCurrentFrame, useVideoConfig } from "remotion";
import { C, Fundo, Sinal } from "../estilo";

export const Titulo: React.FC = () => {
  const f = useCurrentFrame();
  const { fps } = useVideoConfig();
  const aceso = f < 15 ? "verm" : f < 30 ? "amar" : "verde";
  const entra = (d: number) => spring({ frame: f - d, fps, config: { damping: 200 } });
  const linha = (d: number): React.CSSProperties => ({
    opacity: entra(d),
    transform: `translateY(${interpolate(entra(d), [0, 1], [30, 0])}px)`,
  });

  return (
    <Fundo>
      <div style={{ position: "absolute", left: 180, top: 330, display: "flex", gap: 90, alignItems: "center" }}>
        <Sinal aceso={aceso} escala={1.3} />
        <div>
          <div style={{ ...linha(5), fontSize: 92, fontWeight: 800, letterSpacing: -2 }}>
            Controle de AMV com FreeRTOS
          </div>
          <div style={{ ...linha(12), fontSize: 46, color: C.mut, marginTop: 18 }}>
            Aparelho de Mudança de Via · desvio ferroviário
          </div>
          <div style={{ ...linha(19), fontSize: 40, color: C.acc, marginTop: 28, fontWeight: 500 }}>
            STM32 Nucleo-C031C6 · simulado no Wokwi
          </div>
        </div>
      </div>
      <div style={{ ...linha(30), position: "absolute", left: 180, bottom: 110, fontSize: 30, color: C.mut }}>
        Tópicos Especiais II · RTOS embarcados · 2026
      </div>
    </Fundo>
  );
};

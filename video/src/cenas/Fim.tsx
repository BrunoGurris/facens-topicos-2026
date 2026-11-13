import React from "react";
import { spring, useCurrentFrame, useVideoConfig } from "remotion";
import { C, Fundo, Sinal } from "../estilo";

export const Fim: React.FC = () => {
  const f = useCurrentFrame();
  const { fps } = useVideoConfig();
  const s = (d: number) => spring({ frame: f - d, fps, config: { damping: 200 } });
  return (
    <Fundo>
      <div style={{ position: "absolute", left: 180, top: 340, display: "flex", gap: 90, alignItems: "center" }}>
        <Sinal aceso={f < 18 ? "verm" : "verde"} escala={1.3} />
        <div>
          <div style={{ opacity: s(4), fontSize: 80, fontWeight: 800, letterSpacing: -1 }}>Sinal só abre com o AMV travado.</div>
          <div style={{ opacity: s(14), fontSize: 52, color: C.acc, marginTop: 24, fontWeight: 500 }}>
            Qualquer falha leva ao estado seguro.
          </div>
        </div>
      </div>
      <div style={{ opacity: s(24), position: "absolute", left: 180, bottom: 110, fontSize: 30, color: C.mut }}>
        Controle de AMV com FreeRTOS · Tópicos Especiais II · 2026
      </div>
    </Fundo>
  );
};

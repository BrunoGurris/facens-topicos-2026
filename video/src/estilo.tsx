import { loadFont } from "@remotion/google-fonts/Inter";
import React from "react";
import { AbsoluteFill } from "remotion";

export const { fontFamily } = loadFont("normal", {
  weights: ["400", "500", "700", "800"],
  subsets: ["latin", "latin-ext"],
});

export const C = {
  bg: "#181c22",
  bg2: "#22272f",
  fg: "#f0f0ec",
  mut: "#a0a6af",
  linha: "#3a4049",
  acc: "#e8b02e",
  verm: "#d64541",
  amar: "#e8b02e",
  verde: "#4caf50",
};

export const Fundo: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <AbsoluteFill
    style={{
      background: `radial-gradient(circle at 30% 20%, ${C.bg2} 0%, ${C.bg} 70%)`,
      color: C.fg,
      fontFamily,
      fontVariantNumeric: "tabular-nums",
    }}
  >
    {children}
  </AbsoluteFill>
);

/* Sinal ferroviário de 3 focos. */
export const Sinal: React.FC<{ aceso: "verm" | "amar" | "verde" | null; escala?: number }> = ({
  aceso,
  escala = 1,
}) => {
  const focos: ("verm" | "amar" | "verde")[] = ["verm", "amar", "verde"];
  return (
    <div
      style={{
        width: 80 * escala,
        padding: 12 * escala,
        borderRadius: 22 * escala,
        background: "#2a2e36",
        border: `${3 * escala}px solid #555b64`,
        display: "flex",
        flexDirection: "column",
        gap: 12 * escala,
        boxShadow: "0 20px 50px rgba(0,0,0,.4)",
      }}
    >
      {focos.map((f) => (
        <div
          key={f}
          style={{
            width: 56 * escala,
            height: 56 * escala,
            borderRadius: "50%",
            background: aceso === f ? C[f] : "#3b3e45",
            boxShadow: aceso === f ? `0 0 ${30 * escala}px ${C[f]}` : "none",
          }}
        />
      ))}
    </div>
  );
};

import React from "react";
import {
  AbsoluteFill,
  OffthreadVideo,
  interpolate,
  spring,
  staticFile,
  useCurrentFrame,
  useVideoConfig,
} from "remotion";
import { FPS, GRAVACAO, TRECHOS, Trecho, inicioDe } from "../config";
import { C, Fundo, fontFamily } from "../estilo";

const Marcador: React.FC<{ t: Trecho }> = ({ t }) => (
  <Fundo>
    <div
      style={{
        position: "absolute",
        inset: 60,
        border: `4px dashed ${C.linha}`,
        borderRadius: 28,
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        justifyContent: "center",
        gap: 24,
      }}
    >
      <div style={{ fontSize: 34, color: C.mut, letterSpacing: 4 }}>GRAVAÇÃO DA TELA</div>
      <div style={{ fontSize: 56, fontWeight: 700, textAlign: "center", maxWidth: 1400 }}>{t.oQueGravar}</div>
      <div style={{ fontSize: 30, color: C.mut }}>
        trecho "{t.id}" · a partir de {inicioDe(t).toFixed(1)} s da gravação · {t.duracao} s no vídeo
        {t.velocidade && t.velocidade !== 1 ? ` · ${t.velocidade}×` : ""}
      </div>
    </div>
  </Fundo>
);

const Legenda: React.FC<{ texto: string; passo: number }> = ({ texto, passo }) => {
  const f = useCurrentFrame();
  const { fps, durationInFrames } = useVideoConfig();
  const s = spring({ frame: f - 8, fps, config: { damping: 200 } });
  const sai = interpolate(f, [durationInFrames - 12, durationInFrames], [1, 0], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });
  return (
    <div
      style={{
        position: "absolute",
        left: 0,
        right: 0,
        bottom: 70,
        display: "flex",
        justifyContent: "center",
        opacity: s * sai,
        transform: `translateY(${interpolate(s, [0, 1], [40, 0])}px)`,
      }}
    >
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 26,
          background: "rgba(20,22,26,.88)",
          borderLeft: `10px solid ${C.acc}`,
          borderRadius: 16,
          padding: "22px 40px",
          boxShadow: "0 12px 40px rgba(0,0,0,.45)",
        }}
      >
        <span style={{ fontSize: 30, fontWeight: 700, color: C.acc }}>
          {passo}/{TRECHOS.length}
        </span>
        <span style={{ fontSize: 44, fontWeight: 700, color: C.fg }}>{texto}</span>
      </div>
    </div>
  );
};

export const Gravacao: React.FC<{ t: Trecho; passo: number }> = ({ t, passo }) => {
  return (
    <AbsoluteFill style={{ background: "#000", fontFamily }}>
      {GRAVACAO ? (
        <OffthreadVideo
          src={staticFile(GRAVACAO)}
          trimBefore={Math.round(inicioDe(t) * FPS)}
          playbackRate={t.velocidade ?? 1}
          muted
          style={{ width: "100%", height: "100%", objectFit: "contain" }}
        />
      ) : (
        <Marcador t={t} />
      )}
      <Legenda texto={t.legenda} passo={passo} />
    </AbsoluteFill>
  );
};

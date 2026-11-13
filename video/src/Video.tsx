import { TransitionSeries, linearTiming } from "@remotion/transitions";
import { fade } from "@remotion/transitions/fade";
import React from "react";
import { Html5Audio, Sequence, interpolate, staticFile } from "remotion";
import duracoesVoz from "../public/voz/duracoes.json";
import { D_FIM, D_FRENAGEM, D_NUMEROS, D_TITULO, FPS, TRANSICAO, TRECHOS } from "./config";
import { Fim } from "./cenas/Fim";
import { Frenagem } from "./cenas/Frenagem";
import { Gravacao } from "./cenas/Gravacao";
import { Numeros } from "./cenas/Numeros";
import { Titulo } from "./cenas/Titulo";

const q = (s: number) => Math.round(s * FPS);

const CENAS: { id: string; dur: number; el: React.ReactNode }[] = [
  { id: "titulo", dur: q(D_TITULO), el: <Titulo /> },
  ...TRECHOS.map((t, i) => ({ id: t.id, dur: q(t.duracao), el: <Gravacao t={t} passo={i + 1} /> })),
  { id: "frenagem", dur: q(D_FRENAGEM), el: <Frenagem /> },
  { id: "numeros", dur: q(D_NUMEROS), el: <Numeros /> },
  { id: "fim", dur: q(D_FIM), el: <Fim /> },
];

export const DURACAO_TOTAL = CENAS.reduce((a, c) => a + c.dur, 0) - TRANSICAO * (CENAS.length - 1);

/* ---- áudio: narração por cena + música que abaixa durante a fala ---- */
const ATRASO_VOZ = q(0.35);
const VOZ = duracoesVoz as Record<string, number>;
const INICIOS = CENAS.map((_, i) => CENAS.slice(0, i).reduce((a, c) => a + c.dur, 0) - i * TRANSICAO);
const FALAS = CENAS.flatMap((c, i) =>
  VOZ[c.id] ? [[INICIOS[i] + ATRASO_VOZ, INICIOS[i] + ATRASO_VOZ + q(VOZ[c.id])] as const] : [],
);
const MUSICA_ALTA = 0.3;
const MUSICA_BAIXA = 0.1;
const RAMPA = 8;

const volumeMusica = (f: number) => {
  let v = MUSICA_ALTA;
  for (const [a, b] of FALAS) {
    const dentro = Math.min(
      interpolate(f, [a - RAMPA, a], [0, 1], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }),
      interpolate(f, [b, b + RAMPA], [1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" }),
    );
    v = Math.min(v, MUSICA_ALTA - (MUSICA_ALTA - MUSICA_BAIXA) * dentro);
  }
  const fim = interpolate(f, [DURACAO_TOTAL - q(2), DURACAO_TOTAL], [1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
  return v * fim;
};

const Voz: React.FC<{ id: string }> = ({ id }) =>
  VOZ[id] ? (
    <Sequence from={ATRASO_VOZ} layout="none" name={`voz ${id}`}>
      <Html5Audio src={staticFile(`voz/${id}.wav`)} />
    </Sequence>
  ) : null;

export const Apresentacao: React.FC = () => (
  <>
  <Html5Audio src={staticFile("musica.wav")} volume={volumeMusica} />
  <TransitionSeries>
    {CENAS.flatMap((c, i) => [
      <TransitionSeries.Sequence key={c.id} durationInFrames={c.dur} name={c.id}>
        {c.el}
        <Voz id={c.id} />
      </TransitionSeries.Sequence>,
      ...(i < CENAS.length - 1
        ? [<TransitionSeries.Transition key={c.id + "-t"} presentation={fade()} timing={linearTiming({ durationInFrames: TRANSICAO })} />]
        : []),
    ])}
  </TransitionSeries>
  </>
);

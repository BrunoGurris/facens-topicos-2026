import React from "react";
import { interpolate, spring, useCurrentFrame, useVideoConfig, Easing } from "remotion";
import { C, Fundo, Sinal } from "../estilo";

/* Constantes de Core/Inc/amv.h (tTrem). */
const A_SERVICO = 1.2; // m/s²  (TREM_A_SERVICO = 1200 mm/s²)
const A_EMERG = 2.5; //   m/s²  (TREM_A_EMERG   = 2500 mm/s²)
const V0 = 80; //         km/h (TREM_V_MAX_KMH)

const DIST = 250; // m mostrados no eixo; o ponto de parada fica no fim
const X0 = 150, Y0 = 310, W = 1000, H = 470;
const VMAX_EIXO = 90;

const dParada = (a: number) => (V0 / 3.6) ** 2 / (2 * a); // m
const vEm = (pos: number, a: number) => Math.min(V0, Math.sqrt(2 * a * Math.max(0, DIST - pos)) * 3.6);
const px = (pos: number) => X0 + (pos / DIST) * W;
const py = (v: number) => Y0 + H - (v / VMAX_EIXO) * H;

const caminho = (a: number) => {
  const pts: string[] = [];
  for (let pos = 0; pos <= DIST; pos += 0.5) pts.push(`${px(pos).toFixed(1)},${py(vEm(pos, a)).toFixed(1)}`);
  return "M" + pts.join(" L");
};

const clamp = { extrapolateLeft: "clamp", extrapolateRight: "clamp" } as const;

export const Frenagem: React.FC = () => {
  const f = useCurrentFrame();
  const { fps } = useVideoConfig();

  const revServ = interpolate(f, [15, 85], [0, 1], { ...clamp, easing: Easing.inOut(Easing.cubic) });
  const revEmg = interpolate(f, [55, 115], [0, 1], { ...clamp, easing: Easing.inOut(Easing.cubic) });
  const posTrem = interpolate(f, [15, 85], [0, DIST], { ...clamp, easing: Easing.inOut(Easing.cubic) });
  const vTrem = vEm(posTrem, A_SERVICO);
  const cotas = spring({ frame: f - 120, fps, config: { damping: 200 } });

  const dS = dParada(A_SERVICO);
  const dE = dParada(A_EMERG);

  const regras: [string, string, string][] = [
    [C.verm, "Vermelho", "para 5 m antes do sinal"],
    [C.amar, "Amarelo", "entra no ramal a 30 km/h"],
    [C.verm, "Fechou tarde", "frenagem de emergência"],
  ];

  return (
    <Fundo>
      <div style={{ position: "absolute", left: 150, top: 70 }}>
        <div style={{ fontSize: 72, fontWeight: 800, letterSpacing: -1 }}>O trem obedece o sinal</div>
        <div style={{ fontSize: 34, color: C.mut, marginTop: 8 }}>
          curva de frenagem recalculada a cada 50 ms · v = √(2·a·d)
        </div>
      </div>

      <svg width={1920} height={1080} style={{ position: "absolute", inset: 0 }}>
        <defs>
          <clipPath id="cs">
            <rect x={X0} y={0} width={W * revServ + 4} height={1080} />
          </clipPath>
          <clipPath id="ce">
            <rect x={X0} y={0} width={W * revEmg + 4} height={1080} />
          </clipPath>
        </defs>

        {/* grade e eixos */}
        {[0, 20, 40, 60, 80].map((v) => (
          <g key={v}>
            <line x1={X0} x2={X0 + W} y1={py(v)} y2={py(v)} stroke={C.linha} strokeWidth={v === 0 ? 3 : 1} />
            <text x={X0 - 18} y={py(v) + 10} fill={C.mut} fontSize={26} textAnchor="end">
              {v}
            </text>
          </g>
        ))}
        <text x={X0 - 18} y={Y0 - 30} fill={C.mut} fontSize={24} textAnchor="end">
          km/h
        </text>
        <text x={X0 + W / 2} y={Y0 + H + 48} fill={C.mut} fontSize={26} textAnchor="middle">
          distância percorrida até o ponto de parada →
        </text>

        {/* ponto de parada */}
        <line x1={px(DIST)} x2={px(DIST)} y1={Y0 - 10} y2={Y0 + H} stroke={C.verm} strokeWidth={3} strokeDasharray="8 8" />

        {/* curvas */}
        <path d={caminho(A_EMERG)} clipPath="url(#ce)" fill="none" stroke={C.verm} strokeWidth={6} strokeDasharray="18 12" />
        <path d={caminho(A_SERVICO)} clipPath="url(#cs)" fill="none" stroke={C.acc} strokeWidth={8} strokeLinecap="round" />

        {/* trem (marcador) */}
        {f >= 15 && (
          <g transform={`translate(${px(posTrem)}, ${py(vTrem)})`}>
            <circle r={18} fill={C.fg} stroke={C.acc} strokeWidth={6} />
            <text x={posTrem > 215 ? -34 : posTrem < 25 ? 30 : 0} y={posTrem > 215 ? -30 : -40} fill={C.fg} fontSize={34} fontWeight={700} textAnchor={posTrem > 215 ? "end" : posTrem < 25 ? "start" : "middle"}>
              {Math.round(vTrem)} km/h
            </text>
          </g>
        )}

        {/* cotas */}
        <g opacity={cotas}>
          {[
            [dS, C.acc, `serviço 1,2 m/s² → ${Math.round(dS)} m`, Y0 + H + 128],
            [dE, C.verm, `emergência 2,5 m/s² → ${Math.round(dE)} m`, Y0 + H + 205],
          ].map(([d, cor, txt, y]) => (
            <g key={txt as string}>
              <line x1={px(DIST - (d as number))} x2={px(DIST)} y1={y as number} y2={y as number} stroke={cor as string} strokeWidth={4} />
              <line x1={px(DIST - (d as number))} x2={px(DIST - (d as number))} y1={(y as number) - 14} y2={(y as number) + 14} stroke={cor as string} strokeWidth={4} />
              <line x1={px(DIST)} x2={px(DIST)} y1={(y as number) - 14} y2={(y as number) + 14} stroke={cor as string} strokeWidth={4} />
              <text x={px(DIST - (d as number))} y={(y as number) - 24} fill={cor as string} fontSize={30} fontWeight={700}>
                {txt as string}
              </text>
            </g>
          ))}
        </g>
      </svg>

      <div style={{ position: "absolute", left: px(DIST) + 22, top: Y0 - 150 }}>
        <Sinal aceso="verm" escala={0.6} />
      </div>

      {/* regras */}
      <div style={{ position: "absolute", left: 1290, top: 300, display: "flex", flexDirection: "column", gap: 44 }}>
        {regras.map(([cor, t, d], i) => {
          const s = spring({ frame: f - 20 - i * 12, fps, config: { damping: 200 } });
          return (
            <div key={t} style={{ opacity: s, transform: `translateX(${(1 - s) * 40}px)`, display: "flex", gap: 22 }}>
              <div style={{ width: 34, height: 34, borderRadius: "50%", background: cor, marginTop: 8, boxShadow: `0 0 18px ${cor}` }} />
              <div>
                <div style={{ fontSize: 42, fontWeight: 700 }}>{t}</div>
                <div style={{ fontSize: 32, color: C.mut }}>{d}</div>
              </div>
            </div>
          );
        })}
        <div
          style={{
            opacity: spring({ frame: f - 150, fps, config: { damping: 200 } }),
            marginTop: 20,
            padding: "22px 26px",
            borderRadius: 16,
            background: "rgba(232,176,46,.12)",
            border: `2px solid ${C.acc}`,
            fontSize: 32,
            fontWeight: 700,
            color: C.acc,
            maxWidth: 520,
          }}
        >
          Teste a 80 km/h: nenhum avanço do vermelho, nenhum descarrilamento
        </div>
      </div>
    </Fundo>
  );
};

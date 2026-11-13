/*
 * Grava o painel do AMV (tools/painel_amv.py) num Chrome headless enquanto
 * comanda a simulação do Wokwi pelo próprio painel (POST /cmd), seguindo o
 * roteiro do vídeo. Cada ação espera o trem estar no ponto certo (lido pelo SSE).
 *
 * Pré-requisitos: simulador do Wokwi rodando (aba VISÍVEL no VS Code, senão
 * ele pausa) e o painel no ar em http://127.0.0.1:8765.
 *
 *   node gravar.mjs
 *
 * Saída: public/gravacao.mp4 e public/marcas.json (segundo de cada cena e os
 * tempos medidos nesta execução). O config.ts lê o marcas.json.
 */
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import puppeteer from "puppeteer-core";

const PAINEL = process.env.PAINEL ?? "http://127.0.0.1:8765";
const AQUI = path.dirname(new URL(import.meta.url).pathname);
const CHROME = path.join(AQUI, "node_modules/.remotion/chrome-headless-shell/linux64/chrome-headless-shell-linux64/chrome-headless-shell");
const FFMPEG = path.join(AQUI, "node_modules/@remotion/compositor-linux-x64-gnu/ffmpeg");
const QUADROS = path.join(AQUI, "out/quadros");
const SINAL_MM = 295000; // TREM_S_SINAL_MM (amv.h)

/* ---------------- estado vindo do painel (SSE) ---------------- */
const S = { st: {}, tr: {}, tm: null, evs: [], tUlt: 0, hUlt: Date.now() };

async function ouvir() {
  const r = await fetch(`${PAINEL}/events`);
  const dec = new TextDecoder();
  let buf = "";
  for await (const pedaco of r.body) {
    buf += dec.decode(pedaco, { stream: true });
    let i;
    while ((i = buf.indexOf("\n\n")) >= 0) {
      const bloco = buf.slice(0, i);
      buf = buf.slice(i + 2);
      const linha = bloco.split("\n").find((l) => l.startsWith("data: "));
      if (!linha) continue;
      let d;
      try { d = JSON.parse(linha.slice(6)); } catch { continue; }
      if (d.type === "st") S.st = d;
      else if (d.type === "tr") S.tr = d;
      else if (d.type === "tm") S.tm = d;
      else if (d.type === "ev" || d.type === "log") S.evs.push({ h: Date.now(), text: d.text ?? "" });
      if (typeof d.t === "number" && d.t !== S.tUlt) { S.tUlt = d.t; S.hUlt = Date.now(); }
    }
  }
}

const dorme = (ms) => new Promise((r) => setTimeout(r, ms));
async function cmd(c) {
  console.log(`  > ${c}`);
  await fetch(`${PAINEL}/cmd`, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ cmd: c }) });
}
async function ate(desc, cond, maxS = 90) {
  const ini = Date.now();
  while (!cond()) {
    if (Date.now() - S.hUlt > 3000) throw new Error(`simulação parada (aba do Wokwi escondida?) esperando: ${desc}`);
    if (Date.now() - ini > maxS * 1000) throw new Error(`tempo esgotado esperando: ${desc}`);
    await dorme(50);
  }
}
const evDesde = (h, re) => S.evs.some((e) => e.h >= h && re.test(e.text));
const distSinal = () => (S.tr.seg === "volta" ? SINAL_MM - S.tr.s : Infinity);
const longe = () => S.tr.seg === "volta" && S.tr.s < 150000 && !S.st.occ;

/* ---------------- gravação ---------------- */
fs.rmSync(QUADROS, { recursive: true, force: true });
fs.mkdirSync(QUADROS, { recursive: true });

ouvir().catch((e) => { console.error("SSE caiu:", e.message); process.exit(1); });
await ate("telemetria do painel", () => S.st.est && S.tr.seg, 10);

const browser = await puppeteer.launch({ executablePath: CHROME, headless: true, args: ["--no-sandbox"], defaultViewport: { width: 1920, height: 1080 } });
const page = await browser.newPage();
await page.goto(PAINEL, { waitUntil: "networkidle2" });
await dorme(1500);

const cdp = await page.createCDPSession();
const quadros = [];
let h0 = null;
cdp.on("Page.screencastFrame", async ({ data, metadata, sessionId }) => {
  const ts = metadata.timestamp * 1000;
  if (h0 === null) h0 = ts;
  const nome = `q${String(quadros.length).padStart(6, "0")}.jpg`;
  fs.writeFileSync(path.join(QUADROS, nome), Buffer.from(data, "base64"));
  quadros.push({ nome, t: ts });
  await cdp.send("Page.screencastFrameAck", { sessionId }).catch(() => {});
});
await cdp.send("Page.startScreencast", { format: "jpeg", quality: 88, maxWidth: 1920, maxHeight: 1080, everyNthFrame: 1 });
const hIni = Date.now();
const agora = () => (Date.now() - hIni) / 1000;
const marcas = {};
const marca = (id, extra = 0) => { marcas[id] = +(agora() + extra).toFixed(2); console.log(`[${marcas[id]}s] ${id}`); };

try {
  await dorme(1000);

  console.log("1. boot");
  marca("boot", -0.3);
  let h = Date.now();
  await cmd("reiniciar");
  await ate("travado no boot", () => evDesde(h, /travado pos=N/), 20);
  await dorme(2500);

  console.log("2. manobra Reversa");
  await cmd("vel 60");
  await ate("trem longe do AMV", longe, 60);
  marca("manobra", -0.5);
  h = Date.now();
  await cmd("r");
  await ate("travado em R", () => evDesde(h, /travado pos=R/), 10);
  await dorme(1500);

  console.log("3. trem entra no ramal (amarelo)");
  await ate("trem a 120 m do sinal", () => distSinal() < 120000, 60);
  marca("ramal");
  await ate("trem no ramal", () => S.tr.seg === "rota" && S.tr.rota === "R" && S.tr.s > 20000, 40);

  console.log("4. pedido em espera");
  marca("espera", -0.5);
  h = Date.now();
  await cmd("n");
  await ate("pedido em espera", () => S.st.pend && S.st.pend !== "-", 5);
  await ate("pedido executado", () => evDesde(h, /travado pos=N/), 60);
  await dorme(1500);

  console.log("5. emergência com o trem chegando");
  await ate("trem a ~140 m do sinal", () => distSinal() < 140000 && distSinal() > 100000, 90);
  marca("emerg", -0.5);
  await cmd("emg");
  await ate("trem parado", () => S.tr.v === 0, 30);
  await dorme(2000);
  h = Date.now();
  await cmd("rearme");
  await ate("travado após rearme", () => evDesde(h, /travado/), 10);
  await dorme(2500);

  console.log("6. obstrução com o AMV travado");
  await ate("trem longe do AMV", longe, 60);
  marca("obstr", -0.5);
  await cmd("obs 1");
  await ate("falha", () => S.st.falha && S.st.falha !== "nenhuma", 5);
  await dorme(3500);
  await cmd("obs 0");
  await dorme(500);
  h = Date.now();
  await cmd("rearme");
  await ate("travado após rearme", () => evDesde(h, /travado/), 10);
  await dorme(1500);

  console.log("7. tabela de tempos");
  await cmd("stats");
  await page.click("#b-dados");
  marca("dados", -0.3);
  await dorme(5000);
} finally {
  await cdp.send("Page.stopScreencast").catch(() => {});
  await dorme(300);
  await browser.close();
}

/* ---------------- quadros -> mp4 a 30 fps ---------------- */
const lista = quadros.map((q, i) => {
  const dur = ((quadros[i + 1]?.t ?? q.t + 33) - q.t) / 1000;
  return `file '${q.nome}'\nduration ${dur.toFixed(4)}`;
});
lista.push(`file '${quadros.at(-1).nome}'`);
fs.writeFileSync(path.join(QUADROS, "lista.txt"), lista.join("\n"));
// marcas são relativas a hIni; o primeiro quadro chega um pouco depois
const desloc = (h0 - hIni) / 1000;
for (const k of Object.keys(marcas)) marcas[k] = +Math.max(0, marcas[k] - desloc).toFixed(2);

fs.mkdirSync(path.join(AQUI, "public"), { recursive: true });
const tm = S.tm && { ...S.tm, manobra: [S.tm.manobra?.[1] ?? 0, S.tm.manobra?.[1] ?? 0] };
const saida = { marcas, tm, duracao_s: +((quadros.at(-1).t - h0) / 1000).toFixed(1) };
fs.writeFileSync(path.join(AQUI, "public/marcas.json"), JSON.stringify(saida, null, 2));
console.log(JSON.stringify(saida, null, 2));

// o ffmpeg do Remotion não tem o filtro fps: a taxa fixa sai do -fps_mode cfr -r 30
execFileSync(FFMPEG, ["-loglevel", "error", "-y", "-f", "concat", "-safe", "0", "-i", path.join(QUADROS, "lista.txt"),
  "-fps_mode", "cfr", "-r", "30", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", path.join(AQUI, "public/gravacao.mp4")],
  { stdio: ["ignore", "ignore", "inherit"], env: { ...process.env, LD_LIBRARY_PATH: path.dirname(FFMPEG) } });

process.exit(0);

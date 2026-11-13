# Vídeo de apresentação (Remotion)

Vídeo de ~58 s: título → 6 trechos do painel com a simulação rodando → curva de
frenagem animada → tempos medidos × meta → encerramento.

Resultado: `out/apresentacao.mp4` (1920×1080, 30 fps).

## Gerar de novo

```bash
cd video
npm install            # só na primeira vez

# 1. Grava o painel comandando a simulação (≈ 2,5 min, sem intervenção)
#    Precisa: Wokwi rodando com a aba VISÍVEL no VS Code + painel em :8765
node gravar.mjs        # -> public/gravacao.mp4 e public/marcas.json

# 2. Monta o vídeo
npm run render         # -> out/apresentacao.mp4
npm run studio         # (opcional) pré-visualizar/ajustar no navegador
```

`gravar.mjs` abre o painel num Chrome headless, reinicia a placa e segue o roteiro
olhando a posição do trem pelo SSE: pede Reversa com o trem longe, espera ele entrar
no ramal em amarelo, pede Normal com ele sobre o AMV (fica em espera), aciona a
emergência com o trem a ~140 m do sinal (ele para antes do vermelho), rearma, obstrui
a agulha e abre a tabela de tempos. Ele salva o segundo de cada cena e a linha `tm`
da placa em `marcas.json`, e os cortes do vídeo partem dessas marcas.
Os números mostrados (legendas e "Medido × meta") são os da Tabela 14 do relatório, definidos em `src/config.ts`.

Se o Wokwi pausar (aba escondida), o script para com "simulação parada".

## Áudio (narração + música)

Tudo gerado localmente, sem serviço externo:

```bash
pip install --target .pylib piper-tts numpy          # só na primeira vez
mkdir -p .vozes && cd .vozes && for f in pt_BR-faber-medium.onnx pt_BR-faber-medium.onnx.json; do \
  curl -sSLfO "https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/pt/pt_BR/faber/medium/$f"; done; cd ..
python3 gerar_audio.py   # -> public/voz/*.wav, public/voz/duracoes.json, public/musica.wav
npm run render
```

- **Narração:** voz masculina pt-BR "faber" do [Piper](https://github.com/rhasspy/piper)
  (TTS neural offline). Os textos estão em `src/narracao.json`, um por cena. Escreva
  siglas como se fala (ex.: "fri érre tê ó ésse"). O script avisa se uma fala não
  couber na cena.
- **Música:** sintetizada pelo próprio `gerar_audio.py` (Am–F–C–G, 92 BPM), então
  não tem direitos autorais de terceiros. Durante a fala ela abaixa sozinha
  (`MUSICA_ALTA`/`MUSICA_BAIXA` em `src/Video.tsx`).

## Onde ajustar

| Arquivo | O quê |
|---|---|
| `src/config.ts` | legendas, duração/aceleração e deslocamento (`apos`) de cada corte |
| `src/cenas/Frenagem.tsx` | curvas com as constantes de `Core/Inc/amv.h` (1,2 e 2,5 m/s², 80 km/h) |
| `src/cenas/Numeros.tsx` | tabela medido × meta (valores em `TM`, no `config.ts`) |
| `src/cenas/Titulo.tsx`, `Fim.tsx` | abertura e fechamento |
| `gravar.mjs` | roteiro da gravação (ordem dos comandos e gatilhos pela posição do trem) |

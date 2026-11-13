# Vídeo de apresentação (~55 s)

> **O vídeo agora é montado pelo Remotion em [`video/`](../../video/README.md)**: cartelas
> animadas, legendas e cortes saem de `video/src/config.ts`. As PNGs desta pasta ficam
> como alternativa para montar à mão (Kdenlive/Shotcut). O roteiro abaixo vale para os dois.

## Antes de gravar

1. `./build.sh` (compila e abre o painel) e F1 → `Wokwi: Start Simulator`.
2. Tela dividida: **Wokwi à esquerda (~1/3)**, **painel à direita (~2/3)**.
   O Wokwi pausa se a aba ficar escondida, então ela tem que estar visível o tempo todo.
3. Clique em **Reiniciar tudo** e só então comece a gravar (pega o boot).
4. Gravador: GNOME `Ctrl+Shift+Alt+R`, ou OBS. Grave tudo de uma vez (~2–3 min)
   e corte depois. Não precisa acertar o tempo ao vivo.

## Linha do tempo

| Tempo | Imagem | Ação na gravação | Narração (opcional) |
|---|---|---|---|
| 0–4 s | `01_titulo.png` | — | "Controle de um desvio ferroviário com FreeRTOS no STM32." |
| 4–10 s | Boot + `leg_a_boot` | Reiniciar tudo: servo vai a 45°, sinal verde | "Ao ligar, a agulha vai para Normal e só depois o sinal abre." |
| 10–17 s | Manobra + `leg_b_manobra` | **Reversa** | "No pedido, o sinal fecha na hora; a manobra leva 3,1 s e o sinal reabre em amarelo." |
| 17–27 s | Trem freando + `leg_c_trem` | Trem a 40 km/h; quando ele estiver chegando no sinal, clique **Alternar** (sinal fecha durante a manobra) | "Com o sinal vermelho, o trem freia sozinho e para antes do sinal." |
| 27–31 s | Espera + `leg_d_espera` | Peça manobra com o trem em cima do AMV (painel mostra "em espera") | "Com o trem no desvio, o pedido espera: a agulha nunca se move sob o trem." |
| 31–35 s | `leg_e_emerg` | **Emergência** no meio de uma manobra → Rearme | "Emergência: motor para e sinal vermelho em 200 µs." |
| 35–39 s | `leg_f_obstr` | Com o AMV travado, botão **obstruir** no detalhe da agulha | "Agulha obstruída: falha detectada e sinal fechado." |
| 39–46 s | `03_frenagem.png` | — | "O trem usa curva de frenagem: 1,2 m/s² de serviço e 2,5 m/s² de emergência." |
| 46–53 s | `04_numeros.png` | — | "Todos os tempos medidos na placa ficaram dentro da meta." |
| 53–56 s | `05_fim.png` | — | "Sinal só abre com o AMV travado." |

## Dicas

- Se for sem narração, deixe cada legenda pelo menos 3 s na tela.
- No trecho do trem, se ficar lento, acelere a gravação 2× no editor. A frenagem continua visível.
- Os números das cartelas vêm de `docs/secao_5.md` (Tabelas 13 e 14) e das constantes de
  `Core/Inc/amv.h` (`TREM_A_SERVICO = 1200 mm/s²`, `TREM_A_EMERG = 2500 mm/s²`).
  Distância de parada a 80 km/h: v²/2a = 22,2²/2,4 ≈ 206 m (serviço) e /5,0 ≈ 99 m (emergência).
- Editor simples no Linux: Kdenlive ou Shotcut (`sudo apt install kdenlive`). Não há `ffmpeg` instalado aqui.
- Para refazer as cartelas depois de editar os textos: `python3 tools/cartelas_video.py docs/video` (precisa do Pillow).

# Roteiro do vídeo (~6 min)

Antes de gravar: `make`, F1 → `Wokwi: Start Simulator`, `python3 tools/painel_amv.py`.
Deixe o VS Code (circuito do Wokwi) e o painel lado a lado. Clique em **Reiniciar tudo** para zerar as medições.

| Tempo | O que mostrar | O que falar |
|---|---|---|
| 0:00–0:40 | Slide/título, depois o circuito no Wokwi | Grupo, tema: controle de AMV com FreeRTOS no STM32C031C6, tudo no simulador |
| 0:40–1:30 | `docs/prints/visao.png` (ou a seção 4 do relatório) | 7 tarefas, prioridades, filas, mutex com herança, semáforo; regra fail-safe: sinal só abre com AMV travado |
| 1:30–2:10 | Boot: servo vai a 45°, LED verde, painel "TRAVADO" | Na energização a posição é desconhecida, vai para Normal, trava em ~1,6 s |
| 2:10–2:50 | Botão **Reversa** | Sinal vai a vermelho na hora, manobra ~3,1 s, detector confirma, sinal amarelo |
| 2:50–3:30 | **Ocupar via** + pedir Normal; depois desocupar | Pedido fica em espera; ao liberar a via, sai sozinho |
| 3:30–4:20 | **Obstruir** com o AMV travado → **Rearme** (timeout) → desobstruir → **Rearme** | Perda de detecção → vermelho em µs; com obstrução, watchdog em 4,5 s; sem obstrução, trava |
| 4:20–4:50 | **Emergência** no meio de uma manobra → **Rearme** | Motor para, vermelho; volta só com rearme |
| 4:50–5:30 | Trem a 40 e depois 80 km/h; pedir manobra com o trem perto | Trem obedece o sinal (curva de frenagem); aproximação travada impede mover o AMV à frente do trem |
| 5:30–6:00 | Gaveta **☰ Dados**: tabela medido × calculado | Latências medidas na placa (µs) contra a meta de 10 ms; manobra ≈ calculada. **Anotar estes números para a Tabela 14 do relatório** |

Dica: ao final, mande `stats` na serial e tire print — ele dá os números da Tabela 14 e a pilha livre da Tabela 15.

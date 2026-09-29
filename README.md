# AMV: controle de desvio ferroviário com FreeRTOS (STM32 Nucleo-C031C6 no Wokwi)

Trabalho de **Tópicos Especiais II (RTOS embarcados)**. O firmware controla um
**AMV** (Aparelho de Mudança de Via, o desvio de trilho de trem/metrô) com
**FreeRTOS**. Ele faz a manobra da máquina de chave, confirma a posição pelos
detectores de fim de curso, só abre o sinal com o AMV travado e leva tudo ao
estado seguro em falha ou emergência. Tudo roda no simulador
[Wokwi](https://wokwi.com) dentro do VS Code, sem a placa física.

- Um **trem simulado** circula num circuito com o desvio. Ele obedece o sinal
  (curva de frenagem), ocupa o circuito de via e pode passar o vermelho ou
  descarrilar se o sistema falhar. A velocidade é ajustável.
- **7 tarefas** com prioridades fixas e preempção, **5 filas**, **1 mutex**
  (herança de prioridade), **1 semáforo binário**, tudo com alocação estática.
- Latências medidas na própria placa (timer de 1 µs) e mostradas ao lado da
  meta calculada.
- Painel web (seção 5) com o circuito animado, os comandos, o campo de
  velocidade do trem e a tabela de tempo medido × calculado.

A arquitetura, os fluxogramas e os cálculos de tempo estão em
**[docs/ARQUITETURA.md](docs/ARQUITETURA.md)**.

---

## 1. O que você precisa instalar

| Ferramenta | Para quê |
|---|---|
| **Git** | clonar o projeto |
| **VS Code** | editor + simulador |
| **Arm GNU Toolchain** (`arm-none-eabi-gcc`) | compilar o firmware |
| **GNU Make** | rodar o `Makefile` |
| **GDB para ARM** | só se quiser depurar passo a passo (opcional) |
| **Extensão Wokwi** no VS Code | simulador |

Siga a seção do seu sistema operacional e depois pule para a seção 2.

### 1.1 Linux (Ubuntu / Debian / Mint / Pop!_OS)

```bash
sudo apt update
sudo apt install -y git make gcc-arm-none-eabi gdb-multiarch
```

Instale o VS Code (se ainda não tiver):

```bash
sudo snap install code --classic
# ou baixe o .deb em https://code.visualstudio.com/
```

> **Fedora:** `sudo dnf install git make arm-none-eabi-gcc-cs arm-none-eabi-newlib gdb`
> **Arch:** `sudo pacman -S git make arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb`

Verifique:

```bash
arm-none-eabi-gcc --version   # deve mostrar a versão, ex: 14.2.1
make --version
```

### 1.2 Windows 10 / 11

**a) Git** — https://git-scm.com/download/win (instalação padrão).
Isso também instala o **Git Bash**, que é o terminal recomendado para os comandos abaixo.

**b) VS Code** — https://code.visualstudio.com/

**c) Arm GNU Toolchain** — escolha UMA das opções:

- **Opção 1 (mais simples): STM32CubeCLT**
  Baixe em https://www.st.com/en/development-tools/stm32cubeclt.html (pede cadastro gratuito na ST).
  Ele instala de uma vez o `arm-none-eabi-gcc`, o `make`, o `gdb` e o CMake, e já adiciona tudo ao `PATH`.

- **Opção 2: Arm GNU Toolchain + Make separados**
  1. Baixe o instalador `arm-gnu-toolchain-*-mingw-w64-x86_64-arm-none-eabi.exe` em
     https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
  2. Na última tela do instalador, **marque "Add path to environment variable"**.
  3. Instale o Make:
     ```powershell
     winget install GnuWin32.Make
     ```
     (ou `choco install make`, se usar Chocolatey).

**d) Feche e reabra o terminal** (o `PATH` só atualiza em terminais novos) e verifique:

```powershell
arm-none-eabi-gcc --version
make --version
```

Se der `'arm-none-eabi-gcc' não é reconhecido como comando`, o `PATH` não foi atualizado.
Adicione manualmente a pasta `bin` do toolchain (ex.
`C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin`) em
*Configurações → Sistema → Sobre → Configurações avançadas do sistema → Variáveis de ambiente → Path*.

---

## 2. Clonar e compilar

```bash
git clone <URL-DESTE-REPOSITORIO>
cd blink
make -j
```

Ao final deve aparecer algo como:

```
   text    data     bss     dec     hex filename
  28216     328    7952   36496    8e90 build/debug/build/amv.elf
```

e os arquivos `build/debug/build/amv.elf` e `amv.hex` são gerados. São eles que o Wokwi carrega.

> Os avisos `_read is not implemented and will always fail` (e `_write`, `_lseek`, `_close`)
> são normais em firmware bare-metal e podem ser ignorados.

---

## 3. Configurar o VS Code

1. Abra a pasta do projeto: `code .` (ou *File → Open Folder*).
2. Ao abrir, o VS Code vai sugerir instalar as extensões recomendadas — aceite.
   Se não sugerir, instale manualmente (Ctrl+Shift+X):
   - **Wokwi Simulator** (`wokwi.wokwi-vscode`) — obrigatória
   - **C/C++** (`ms-vscode.cpptools`) — autocompletar e depuração

   Ou pelo terminal:
   ```bash
   code --install-extension wokwi.wokwi-vscode
   code --install-extension ms-vscode.cpptools
   ```
3. **Recarregue a janela** depois de instalar: F1 → `Developer: Reload Window`.
4. Ative a licença gratuita do Wokwi (só na primeira vez):
   F1 → `Wokwi: Request a new License` → faz login no navegador (GitHub/Google) → a licença é
   ativada automaticamente no VS Code.

---

## 4. Rodar a simulação

1. Compile: `make` no terminal, ou **Ctrl+Shift+B** no VS Code (tarefa `Build`).
2. F1 → **`Wokwi: Start Simulator`**.

No boot o servo sai de 90° (posição desconhecida) e vai para **Normal (45°)**.
Quando o detector confirma, o sinal fica **verde**. No monitor serial aparece:

```
AMV: controle de desvio ferroviario (FreeRTOS V11.1.0)
st t=250 est=movendo cmd=N det=- sig=vermelho ang=8250 occ=0 obs=0 falha=nenhuma
...
ev t=1612 travado pos=N manobra_ms=1612
st t=1750 est=travado cmd=N det=N sig=verde ang=4500 occ=0 obs=0 falha=nenhuma
```

### O circuito (diagram.json)

| Componente | Pino | Papel |
|---|---|---|
| Servo | PA6 (TIM3_CH1) | máquina de chave: 45° = Normal, 135° = Reversa |
| LEDs vermelho / amarelo / verde | PB0 / PA7 / PB6 | sinal: verde = via direta livre, amarelo = desviada, vermelho = pare |
| LEDs Det N / Det R | PB4 / PB5 | contatos dos detectores (**simulados** pela tarefa `tPlanta`) |
| Fios PB4→PA0 e PB5→PA1 | PA0 / PA1 (EXTI) | o controle recebe a detecção por interrupção, como receberia do campo |
| Chave "Ocupação" | PA4 | trem sobre o AMV: proíbe manobra |
| Chave "Obstrução" | PB1 | injeta falha: a agulha não encosta e o detector não confirma |
| Botão verde N/R | PA10 (EXTI) | pede manobra (alterna Normal/Reversa) |
| Botão amarelo Rearme | PB3 | rearma depois de falha ou emergência |
| Botão vermelho Emergência | PC13 (EXTI) | estado seguro imediato |
| LD4 da placa | PA5 | pisca a 1 Hz: CPU e escalonador vivos |

### Comandos pela serial

| Comando | Efeito |
|---|---|
| `n` / `normal`, `r` / `reversa`, `t` / `alternar` | pede manobra |
| `emg` | emergência |
| `rearme` | rearme após falha/emergência (continua a mesma execução) |
| `reiniciar` | **reset completo da placa**: tempo, medições e estado voltam do zero (boot de novo) |
| `vel <km/h>` | velocidade do trem simulado (0 a 80; `vel 0` para) |
| `trem` | recoloca o trem nos trilhos (depois de descarrilar), parado no início da linha |
| `occ 1` / `occ 0` | ocupa o circuito de via sem trem (soma com a chave física e com o trem) |
| `obs 1` / `obs 0` | simula agulha obstruída |
| `status` | linha `st` na hora |
| `stats` | tempos medidos (`tm`) e pilha livre de cada tarefa (`stk`) |
| `help` | lista os comandos |

A placa responde `ok <comando>` ou `err <motivo>`. Depois vêm os eventos
(`ev ... pedido`, `pendente`, `travado`, `rejeitado`, `falha`, `emergencia`, `rearme`).

Com o trem no circuito de via (ou o AMV em movimento), o pedido **fica em
espera** e é executado sozinho quando a via libera. O painel mostra o botão com
borda tracejada e a mensagem "Pedido para … em espera".

**Sempre que alterar o código, rode `make` de novo antes de reiniciar o simulador.**

---

## 5. Painel web: pátio, comandos e tempos

O script `tools/painel_amv.py` conecta na serial da placa simulada e abre uma
página no navegador com:

- o **circuito animado**: linha principal oval, ramal da rota Reversa, sinal aceso, o trem andando e o
  trecho do circuito de via (vermelho quando ocupado);
- o **detalhe da agulha** (vista de cima, no centro do oval): trilhos de encosto, as duas agulhas
  móveis seguindo o ângulo real do servo, tirante, máquina de chave, detectores N/R, estado
  (TRAVADO / MOVENDO / FALHA / EMERGÊNCIA) e o pedido em espera;
- o campo de **velocidade do trem** (0 a 80 km/h), o botão Parar e o "Recolocar trem";
- os botões **Normal / Reversa / Alternar / Emergência / Rearme / Reiniciar tudo** e os de simular trem e obstrução.
  "Reiniciar tudo" reseta a placa e o painel limpa o gráfico, o log e a tabela de tempos;
- a tabela **tempo medido × tempo calculado** (latência da emergência, da
  perda de detecção, do pedido, duração da manobra e jitter do motor) com ✓ ou ✕;
- o gráfico do ângulo da máquina de chave, colorido pelo aspecto do sinal, e o gráfico da
  velocidade do trem, colorido pelo freio (tração / serviço / emergência);
- o log de eventos.

Como funciona: o `wokwi.toml` tem `rfc2217ServerPort = 4000`, que faz o Wokwi
expor a USART2 num servidor TCP. O script conecta nele com `pyserial` e
interpreta as linhas `st`/`ev`/`tm`/`tr`/`stk`. Os botões fazem `POST /cmd`, e o
script escreve o comando na serial.

Só precisa do `pyserial`:

```bash
sudo apt install python3-serial      # Linux
pip install pyserial                 # Windows / venv
```

Atalho: `./build.sh` compila o firmware e já abre o painel
(`./build.sh build` só compila, `./build.sh clean` limpa). No Windows use o Git Bash.

Passo a passo equivalente:

1. `make` e F1 → `Wokwi: Start Simulator` (o servidor da serial só sobe com o simulador).
2. Em outro terminal: `python3 tools/painel_amv.py` (Windows: `python tools\painel_amv.py`),
   ou F1 → `Tasks: Run Task` → **Painel do AMV**.
3. O navegador abre sozinho. Se o simulador ainda não estiver rodando, a página
   fica em "sem conexão" e conecta assim que ele iniciar.

Opções: `--serial /dev/ttyACM0` (ou `COM3`) para uma placa física,
`--http 9000` para trocar a porta da página, `--no-open` para não abrir o navegador.

O Chart.js está incluído em `tools/chart.umd.min.js`, então tudo funciona offline.

---

### 5.1 Controle pelo celular (PWA + ngrok)

Em `/m` existe uma versão para celular que pode ser **instalada como app**
(PWA). Ela tem botões grandes (Normal, Reversa, Emergência, Rearme), um slider
de velocidade do trem, o mini-mapa do circuito com o trem andando, o estado do
AMV, o pedido em espera e os últimos eventos. Os botões de simulação (ocupar
via, obstruir, recolocar trem, reiniciar) ficam na seção recolhível "Simulação".

Para acessar de fora do PC, use o [ngrok](https://ngrok.com). O PWA precisa
de HTTPS, e o ngrok já entrega:

```bash
./build.sh run --senha minhasenha     # ou: python3 tools/painel_amv.py --senha minhasenha
ngrok http 8765                       # em outro terminal
```

No celular, abra `https://<endereço-do-ngrok>/m`, digite a senha (fica salva
no aparelho) e instale:
- **Android (Chrome):** botão "Instalar app na tela inicial" da página, ou menu ⋮ → Instalar app;
- **iPhone (Safari):** Compartilhar → Adicionar à Tela de Início.

Observações:
- **Use `--senha`.** Sem ela, qualquer pessoa com o link do ngrok comanda o
  simulador. O servidor reconhece acesso pelo túnel (cabeçalho `X-Forwarded-For`)
  e exige a senha só dele. O painel local em `http://localhost:8765` continua livre.
- O plano grátis do ngrok mostra uma página de aviso na primeira visita: toque em "Visit Site".
- O app instalado fica preso ao endereço. Como o endereço do ngrok muda a cada
  execução, use o **domínio estático grátis** do ngrok
  (`ngrok http --url=<seu-dominio>.ngrok-free.app 8765`) para não ter que reinstalar.
- O painel completo (`/`) é para uso local: pelo túnel ele não tem a tela de senha.

## 6. Testes automáticos no PC

```bash
make test          # ou: tests/run.sh [cenario ...]
```

Compila as tarefas **reais** do AMV (`amv_tasks.c`, `amv_trem.c`, `amv_comm.c`,
sem alterar nada) com o **kernel FreeRTOS de verdade**, usando o port POSIX
(cada tarefa vira uma thread do Linux). O hardware é substituído por
`tests/host/hw_host.c`: os contatos da planta chegam nos "pinos" dos detectores
como no fio do `diagram.json`, e cada borda vira o mesmo evento que a ISR gera.
Cada cenário de `tests/cenarios.c` manda comandos pela "serial", como o painel
faria. O `run.sh` confere as linhas que a placa responde. Leva ~1,5 min, com
todos os cenários rodando em paralelo e em tempo real.

| Cenário | O que exercita |
|---|---|
| `manobras` | manobras alternadas com o trem circulando a 40 km/h |
| `trem80` | trem a 80 km/h (circuito ocupado ~2/3 do tempo): pedidos saem da fila |
| `cliques` | vários pedidos em cima de uma manobra, Alternar, cancelamento da espera |
| `emergencia` | emergência no meio da manobra, rearme, manobras depois |
| `fila` | circuito ocupado guarda o pedido; liberou, executa sozinho |
| `obstrucao` | perda de detecção, rearme com obstrução (timeout), rearme sem obstrução |
| `fio_quebrado` | controle: fio PB5→PA1 rompido tem que dar timeout com `planta=R pinos=-` |

Logs completos em `tests/out/<cenario>.log` (com `HIL_VERBOSE=1` incluem também
as linhas periódicas `st`/`tr`). Os tempos são do PC, não da placa, então
servem para validar a lógica. Os números do relatório vêm do Wokwi.
Só roda em Linux/macOS (pthreads) e precisa do `gcc` nativo (`sudo apt install build-essential`).

---

## 7. Depurar (opcional)

1. F1 → `Wokwi: Start Simulator and Wait for Debugger`
2. Vá em *Run and Debug* (Ctrl+Shift+D), escolha **Wokwi Debug (Linux)** ou
   **Wokwi Debug (Windows)** conforme seu sistema, e aperte **F5**.
3. Coloque breakpoints em `Core/Src/main.c` normalmente.

O Wokwi expõe um servidor GDB na porta 3333 (configurado em `wokwi.toml`).

---

## 8. Estrutura do projeto

```
blink/
├── Core/
│   ├── Inc/
│   │   ├── main.h            # mapa de pinos (Det_Normal_Pin, Servo_Pin, Sinal_*...)
│   │   ├── amv.h             # tipos, tempos (T_MANOBRA_MS...), prioridades, filas
│   │   └── FreeRTOSConfig.h  # configuração do kernel (tick 1 ms, estático, preemptivo)
│   └── Src/
│       ├── main.c            # init do HAL/GPIO/UART e chama amv_start()
│       ├── amv_tasks.c       # AS TAREFAS: tSeg, tMotor, tInter, tSinal, tPlanta
│       ├── amv_comm.c        # tComm: protocolo serial e telemetria
│       ├── amv_trem.c        # tTrem: SIMULAÇÃO do trem (frenagem, ocupação)
│       ├── amv_hw.c          # servo (TIM3), cronômetro 1 µs (TIM14), ISRs → filas
│       └── stm32c0xx_it.c    # vetores de interrupção (EXTI, USART2)
├── Middlewares/Third_Party/FreeRTOS/   # kernel FreeRTOS V11.1.0 (MIT), ports ARM_CM0 e POSIX (testes)
├── Drivers/                  # HAL da ST + CMSIS (não mexer)
├── docs/ARQUITETURA.md       # processo, tarefas, fluxogramas, cálculos de tempo
├── tests/                    # testes no PC: cenarios.c, host/ (HW falso), run.sh
├── build.sh                  # compila e abre o painel web (./build.sh)
├── tools/
│   ├── painel_amv.py         # ponte serial ↔ navegador
│   ├── painel_amv.html       # a página do painel
│   ├── pwa/                  # controle mobile (/m): controle.html, manifest, sw.js, ícones
│   └── chart.umd.min.js      # Chart.js (offline)
├── diagram.json              # circuito simulado: placa, servo, sinal, chaves, botões
├── wokwi.toml                # firmware, porta GDB e porta da serial (RFC2217)
├── Makefile                  # build (make / make clean)
├── STM32C031C6Tx_FLASH.ld    # mapa de memória do chip
├── startup_stm32c031xx.s     # código de boot
├── blink.ioc                 # projeto CubeMX original (NÃO reflete os pinos novos)
└── .vscode/                  # tarefas, debug e extensões recomendadas
```

> O `blink.ioc` é do projeto original. Os pinos do AMV foram configurados à mão
> em `main.c`/`main.h`. Se você regerar o código pelo CubeMX, o código entre os
> `USER CODE BEGIN/END` é preservado, mas o `MX_GPIO_Init` e o
> `stm32c0xx_it.c` voltariam ao original.

### Editando o circuito

O `diagram.json` pode ser escrito à mão ou montado visualmente em https://wokwi.com
(novo projeto → placa *ST Nucleo C031C6* → arrastar componentes → copiar o `diagram.json` gerado).
Lista de componentes: https://docs.wokwi.com/parts/

---

## 9. Criando um novo projeto a partir deste

```bash
cp -r blink meu-projeto
cd meu-projeto
rm -rf build
sed -i 's/^TARGET = amv/TARGET = meu-projeto/' Makefile
sed -i 's#build/amv\.#build/meu-projeto.#g' wokwi.toml .vscode/launch.json build.sh
make
```

No Windows (Git Bash) os mesmos comandos funcionam.

---

## Problemas comuns

| Sintoma | Causa / solução |
|---|---|
| Comandos `Wokwi:` não aparecem no F1 | Extensão não carregou. F1 → `Developer: Reload Window` |
| `Wokwi: firmware not found` | Rode `make`; confira se `build/debug/build/amv.hex` existe |
| `arm-none-eabi-gcc: command not found` | Toolchain não instalado ou fora do `PATH` (seção 1) |
| `make: command not found` (Windows) | Instale o Make (seção 1.2c) e reabra o terminal |
| Simulador abre mas nada no serial | Confira `diagram.json`: `$serialMonitor` ligado em PA2/PA3 |
| Painel fica em "sem conexão" | Simulador não está rodando, ou `wokwi.toml` sem `rfc2217ServerPort = 4000` |
| `No module named 'serial'` | Instale o pyserial (seção 5) |
| Botões do painel desabilitados | Sem conexão com a serial; ao conectar eles habilitam sozinhos |
| Pede licença de novo | F1 → `Wokwi: Request a new License` |
| `falha tipo=timeout` ao manobrar | leia os campos do evento: `ang` diferente de 4500/13500 → o motor não chegou; `obs=1` → obstrução ligada (chave "Obstrução" do Wokwi ou botão do painel); `planta=R pinos=-` → o contato foi gerado mas não chegou em PA0/PA1: confira os fios PB4→PA0 e PB5→PA1 no `diagram.json` |
| Sinal nunca sai do vermelho | a chave "Obstrução" está ligada, ou houve falha/emergência: tire a obstrução e mande `rearme` |
| Pedido fica eternamente "em espera" | a chave "Ocupação" do Wokwi está ligada, ou o botão "Ocupar via" do painel (`occ 1`); desligue |
| LEDs congelados e LD4 parou de piscar | `configASSERT` ou estouro de pilha: o firmware entra no estado seguro e para. Use o GDB (seção 7) |

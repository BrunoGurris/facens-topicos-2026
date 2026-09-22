# blink — STM32 Nucleo-C031C6 simulado no Wokwi (VS Code)

Projeto base para desenvolver firmware STM32 e testá-lo no simulador
[Wokwi](https://wokwi.com) direto do VS Code, sem precisar da placa física.

O exemplo pisca um LED em **PA5** a cada 500 ms, imprime o timer da placa e o
estado do LED no monitor serial, e inverte o LED quando o botão em **PC13** é
pressionado. Um script em `tools/` abre um painel web com o timer em gráficos
em tempo real e botões para controlar a placa pela serial (seção 5).

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
   6448      20    1788    8256    2040 build/debug/build/blink.elf
```

e os arquivos `build/debug/build/blink.elf` e `blink.hex` são gerados. São eles que o Wokwi carrega.

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

Deve abrir uma aba com a placa Nucleo, o LED piscando, e o monitor serial mostrando:

```
Blink iniciado. Pressione o botao para inverter o LED.
Envie 'help' pela serial para ver os comandos.
tick=0 led=1 blink=1 period=500
tick=500 led=0 blink=1 period=500
tick=1000 led=1 blink=1 period=500
...
```

`tick` é o `HAL_GetTick()`: milissegundos desde o boot, contados pelo SysTick.

A placa também aceita comandos digitados no monitor serial (ou enviados pelo painel web):

| Comando | Efeito |
|---|---|
| `led on` / `led off` / `led toggle` | controla o LED diretamente |
| `blink on` / `blink off` | liga/desliga a piscada automática |
| `period <ms>` | período da piscada (20 a 10000 ms) |
| `status` | reenvia a linha `tick=...` |
| `help` | lista os comandos |

A resposta é `ok <comando>` ou `err <motivo>`.

Clique no botão azul do diagrama para inverter o LED (aparece `Botao pressionado!`).

**Sempre que alterar o código, rode `make` de novo antes de reiniciar o simulador.**

---

## 5. Painel web: gráfico do timer e controle da placa

O script `tools/plot_timer.py` conecta na serial da placa simulada e abre uma
página no navegador com:

- gráficos ao vivo do `HAL_GetTick()`, do intervalo entre amostras e do estado do LED;
- botões para ligar/desligar/inverter o LED, ligar/desligar a piscada e mudar o
  período, além de um campo para comando livre;
- o log das respostas da placa (`ok ...` / `err ...`).

Como funciona: o `wokwi.toml` tem `rfc2217ServerPort = 4000`, que faz o Wokwi
expor a USART2 num servidor TCP bidirecional. O script conecta nele com
`pyserial`, parseia as linhas `tick=...` para os gráficos e, quando você clica
num botão, a página faz `POST /cmd` e o script escreve o comando na serial —
o firmware lê a RX por polling no loop principal e executa.

Só precisa do `pyserial`:

```bash
sudo apt install python3-serial      # Linux
pip install pyserial                 # Windows / venv
```

Rodar (atalho): `./build.sh` compila o firmware e já abre o painel
(`./build.sh build` só compila, `./build.sh clean` limpa). No Windows use o Git Bash.

Passo a passo equivalente:

1. `make` e F1 → `Wokwi: Start Simulator` (o servidor da serial só sobe com o simulador).
2. Em outro terminal: `python3 tools/plot_timer.py` (Windows: `python tools\plot_timer.py`),
   ou F1 → `Tasks: Run Task` → **Grafico do timer**.
3. O navegador abre sozinho. Se o simulador ainda não estiver rodando, a página
   fica em "sem conexão" e conecta assim que ele iniciar.

Opções: `--serial /dev/ttyACM0` (ou `COM3`) para uma placa física,
`--http 9000` para trocar a porta da página, `--no-open` para não abrir o navegador.

O Chart.js está incluído em `tools/chart.umd.min.js`, então tudo funciona offline.

---

## 6. Depurar (opcional)

1. F1 → `Wokwi: Start Simulator and Wait for Debugger`
2. Vá em *Run and Debug* (Ctrl+Shift+D), escolha **Wokwi Debug (Linux)** ou
   **Wokwi Debug (Windows)** conforme seu sistema, e aperte **F5**.
3. Coloque breakpoints em `Core/Src/main.c` normalmente.

O Wokwi expõe um servidor GDB na porta 3333 (configurado em `wokwi.toml`).

---

## 7. Estrutura do projeto

```
blink/
├── Core/
│   ├── Inc/main.h          # defines dos pinos (Led_Pin, User_Button_Pin...)
│   └── Src/main.c          # SEU CÓDIGO vai aqui (entre USER CODE BEGIN/END)
├── Drivers/                # HAL da ST + CMSIS (não mexer)
├── build.sh                # compila e abre o painel web (./build.sh)
├── tools/
│   ├── plot_timer.py       # ponte serial ↔ navegador (gráficos + comandos)
│   ├── plot_timer.html     # a página do painel
│   └── chart.umd.min.js    # Chart.js (biblioteca de gráficos, offline)
├── diagram.json            # circuito simulado: placa, LED, botão, fios
├── wokwi.toml              # firmware, porta GDB e porta da serial (RFC2217)
├── Makefile                # build (make / make clean)
├── STM32C031C6Tx_FLASH.ld  # mapa de memória do chip
├── startup_stm32c031xx.s   # código de boot
├── blink.ioc               # projeto do STM32CubeMX (para regerar init de periféricos)
└── .vscode/                # tarefas, debug e extensões recomendadas
```

### Pinos usados

| Pino | Função |
|---|---|
| PA5  | LED (`Led_Pin`) |
| PC13 | Botão (`User_Button_Pin`), interrupção EXTI |
| PA2  | USART2 TX → monitor serial |
| PA3  | USART2 RX ← monitor serial |

### Editando o circuito

O `diagram.json` pode ser escrito à mão ou montado visualmente em https://wokwi.com
(novo projeto → placa *ST Nucleo C031C6* → arrastar componentes → copiar o `diagram.json` gerado).
Lista de componentes: https://docs.wokwi.com/parts/

---

## 8. Criando um novo projeto a partir deste

```bash
cp -r blink meu-projeto
cd meu-projeto
rm -rf build
mv blink.ioc meu-projeto.ioc
sed -i 's/blink/meu-projeto/g' Makefile CMakeLists.txt wokwi.toml .vscode/launch.json .project .cproject meu-projeto.ioc
make
```

No Windows (Git Bash) os mesmos comandos funcionam.

---

## Problemas comuns

| Sintoma | Causa / solução |
|---|---|
| Comandos `Wokwi:` não aparecem no F1 | Extensão não carregou. F1 → `Developer: Reload Window` |
| `Wokwi: firmware not found` | Rode `make`; confira se `build/debug/build/blink.hex` existe |
| `arm-none-eabi-gcc: command not found` | Toolchain não instalado ou fora do `PATH` (seção 1) |
| `make: command not found` (Windows) | Instale o Make (seção 1.2c) e reabra o terminal |
| Simulador abre mas nada no serial | Confira `diagram.json`: `$serialMonitor` ligado em PA2/PA3 |
| Gráfico fica em "sem conexão" | Simulador não está rodando, ou `wokwi.toml` sem `rfc2217ServerPort = 4000` |
| `No module named 'serial'` | Instale o pyserial (seção 5) |
| Botões do painel desabilitados | Sem conexão com a serial; ao conectar eles habilitam sozinhos |
| Pede licença de novo | F1 → `Wokwi: Request a new License` |

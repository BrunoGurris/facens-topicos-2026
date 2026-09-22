# blink — STM32 Nucleo C031C6 no Wokwi

Projeto base derivado do `stm32-hello-wokwi`. Pisca o LED em PA5 a cada 500 ms,
imprime o estado na serial e inverte o LED quando o botao (PC13) e pressionado.

## Ciclo de trabalho

1. Editar `Core/Src/main.c` (sempre entre os marcadores `USER CODE BEGIN/END`).
2. Editar `diagram.json` para mudar o circuito (componentes e fios).
3. Compilar: `make`
4. No VS Code: **F1 -> Wokwi: Start Simulator**

## Pinos usados

| Pino | Funcao                      |
|------|-----------------------------|
| PA5  | LED (`Led_Pin`)             |
| PC13 | Botao (`User_Button_Pin`)   |
| PA2  | USART2 TX (monitor serial)  |
| PA3  | USART2 RX (monitor serial)  |

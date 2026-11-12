# Texto para inserir em Tópicos2.docx (capítulo 5)

> Valores dos testes automáticos (`make test`) e do roteiro executado no Wokwi em 11/11/2026, ambos sobre o código atual. Falta só o print da Figura 6 (marcado com [PREENCHER]).

## 5 TESTES E RESULTADOS

A validação do sistema foi realizada em duas etapas complementares. Na primeira, a lógica de controle foi verificada por meio de testes automáticos executados no computador, nos quais as tarefas reais da aplicação são compiladas, sem nenhuma alteração, sobre o kernel FreeRTOS em seu port POSIX. Na segunda, o firmware completo foi executado no simulador Wokwi, com o microcontrolador STM32C031C6 emulado, e as latências foram medidas pela própria placa com o temporizador de 1 µs descrito na seção 4.5. A primeira etapa responde à pergunta "o sistema faz a coisa certa?", e a segunda, "o sistema faz a coisa certa dentro do prazo?".

### 5.1 TESTES AUTOMÁTICOS DA LÓGICA

No ambiente de teste, cada tarefa do FreeRTOS é executada como uma thread do sistema operacional, e o hardware é substituído por uma camada simulada: os contatos gerados pela tarefa de planta chegam aos "pinos" dos detectores da mesma forma que pelo fio do circuito do Wokwi, e cada borda produz o mesmo evento que a rotina de interrupção geraria. Uma tarefa adicional faz o papel do operador, enviando comandos pela fila de recepção da serial como se fossem bytes chegando pela USART2. Um script compara as linhas emitidas pelo sistema com padrões esperados (eventos que devem ocorrer, com número mínimo de ocorrências, e eventos que não podem ocorrer). Os sete cenários elaborados estão descritos na Tabela 11.

**Tabela 11 - Cenários de teste automático**

| Cenário | Situação exercitada | Critério de aprovação |
|---|---|---|
| manobras | Oito pedidos alternados com o trem circulando a 40 km/h | Ao menos 3 travamentos em cada posição e nenhuma falha |
| trem80 | Pedidos a cada 4 s com o trem a 80 km/h (circuito ocupado cerca de 2/3 do tempo) | Ao menos 3 travamentos em Reversa, pedidos executados a partir da espera e nenhuma falha |
| cliques | Vários pedidos sobre uma manobra em curso e comando Alternar repetido | Pedidos guardados em espera, cancelamento da espera por novo pedido e nenhuma falha |
| emergencia | Emergência no meio de uma manobra, seguida de rearme e novas manobras | Evento de emergência com latência registrada, rearme e manobras posteriores travadas |
| fila | Pedido com o circuito de via ocupado e posterior liberação | Pedido em espera com motivo "ocupado" e execução automática após a liberação |
| obstrucao | Obstrução com o AMV travado, rearme com obstrução e rearme sem obstrução | Falha por perda de detecção, falha por timeout com obstrução indicada e novo travamento |
| fio_quebrado | Fio PB5→PA1 rompido (teste de controle do diagnóstico) | Falha por timeout com diagnóstico "planta=R pinos=-" e nenhum travamento em Reversa |

*Fonte - Próprio Autor (2026).*

O cenário fio_quebrado funciona como um teste de controle: ele injeta propositalmente um defeito na ligação entre o contato da planta e a entrada do detector, verificando que o sistema não confirma uma posição que não foi efetivamente detectada e que o diagnóstico emitido aponta corretamente o elo rompido da cadeia. Todos os cenários foram executados em paralelo, em tempo real, com duração total de aproximadamente 1,5 minuto, e os resultados obtidos estão na Tabela 12.

**Tabela 12 - Resultados dos testes automáticos**

| Cenário | Resultado | Manobras concluídas | Duração da manobra (último / máximo) | Observação |
|---|---|---|---|---|
| manobras | Aprovado | 7 | 3.100 / 3.109 ms | Um pedido ficou em espera enquanto o trem ocupava o circuito |
| trem80 | Aprovado | 7 | 3.100 / 3.109 ms | Dois pedidos executados a partir da espera (após 1.000 ms e 100 ms) |
| cliques | Aprovado | 5 | 3.108 / 3.108 ms | Pedido para a posição atual cancelou a espera, como projetado |
| emergencia | Aprovado | 4 | 3.100 / 3.108 ms | Após o rearme, a manobra interrompida foi concluída em 1.629 ms |
| fila | Aprovado | 2 | 3.105 ms | Pedido executado sozinho 3.052 ms após o registro, logo após a liberação |
| obstrucao | Aprovado | 2 | – | Perda de detecção, timeout em 4.500 ms com obs=1 e travamento após o rearme |
| fio_quebrado | Aprovado | 1 (boot) | – | Timeout em 4.500 ms com diagnóstico "ang=13500 planta=R pinos=-" |

*Fonte - Próprio Autor (2026).*

Em todos os cenários a duração das manobras completas de Normal para Reversa e vice-versa ficou entre 3.100 e 3.109 ms, dentro da faixa calculada na seção 3.1.2 (3.100 a 3.120 ms) e com folga de cerca de 1.390 ms em relação ao limite do watchdog. A manobra de inicialização (de 90° para 45°) durou 1.600 ms em todas as execuções, também conforme o cálculo. Nos dois cenários de falha, o watchdog atuou exatamente aos 4.500 ms, e o diagnóstico registrado distinguiu as duas causas: no caso da obstrução, o motor chegou ao fim do curso, mas a planta não gerou o contato (planta=-, obs=1); no caso do fio rompido, a planta gerou o contato da posição reversa, mas ele não chegou ao pino de entrada (planta=R, pinos=-).

Cabe destacar que, nesse ambiente, os tempos em microssegundos não representam o desempenho do microcontrolador, pois o "hardware" é um processo do Linux sujeito ao escalonador do sistema operacional hospedeiro. Por esse motivo, os testes automáticos foram usados exclusivamente para validar a lógica, e as latências apresentadas na seção 5.2 foram obtidas no simulador.

### 5.2 MEDIÇÕES NO SIMULADOR

No simulador Wokwi, o firmware foi submetido ao roteiro de testes funcionais da Tabela 13. Os comandos foram enviados pela serial, por meio do painel web, e os eventos emitidos pela placa foram registrados ao longo de toda a execução. A placa foi reiniciada no início de cada execução do roteiro, de modo que todas as medições de uma rodada se referem a uma mesma execução.

#### 5.2.1 Primeira rodada: falha na supervisão do AMV travado

Na primeira execução do roteiro, nove dos dez passos tiveram o resultado esperado, mas o passo 4 falhou: com o AMV travado, a obstrução da agulha não levou o sistema à falha, e nenhum evento foi registrado durante 30 s. A investigação, feita pela leitura dos registradores do microcontrolador por meio do servidor de depuração do Wokwi, mostrou que as interrupções externas (EXTI) dos pinos dos detectores ficavam registradas como pendentes, mas a rotina de interrupção correspondente nunca era executada no simulador. O mesmo ocorria com a linha do botão de comando (PA10). Como a configuração das interrupções no firmware estava correta e a interrupção da USART2 funcionava normalmente, o comportamento indica uma limitação do simulador no tratamento das interrupções externas do STM32C031C6.

Durante a manobra, essa limitação foi mascarada pela redundância prevista na seção 3: a confirmação de posição ocorreu sempre pela leitura periódica dos detectores (registrada como "confirmado=nivel"), o que acrescentou até 50 ms às manobras, que duraram 3.150 ms. Com o AMV travado, porém, não havia leitura periódica, e a supervisão dependia exclusivamente da interrupção. O teste evidenciou, portanto, uma falha de projeto: a supervisão do AMV travado possuía um único caminho, ao contrário da confirmação da manobra. A mesma rodada mostrou ainda que a tarefa de comunicação havia utilizado 248 das 256 palavras de sua pilha, restando apenas 8.

#### 5.2.2 Correções aplicadas

Foram feitas duas alterações no firmware. A primeira adiciona um segundo caminho para as interrupções externas: o gancho de tick do FreeRTOS (vApplicationTickHook), executado a cada 1 ms, verifica se alguma linha EXTI dos detectores ou dos botões está pendente sem ter sido atendida e, nesse caso, executa o mesmo tratamento da rotina de interrupção. Na placa física, a rotina de interrupção limpa o indicador antes, e o gancho não tem efeito; no simulador, ele garante que a perda de detecção seja tratada em até 1 ms, ainda dentro da meta de 10 ms. A verificação é feita em seção crítica, para que a rotina de interrupção e o gancho não tratem o mesmo evento. A segunda alteração amplia a pilha da tarefa de comunicação de 256 para 384 palavras. Com isso, o consumo de RAM do firmware passou de 8.280 para 8.792 bytes, e os sete cenários de teste automático da seção 5.1 continuaram aprovados.

#### 5.2.3 Segunda rodada: resultados após as correções

Com o firmware corrigido, o roteiro foi executado novamente, com 14 manobras ao todo. Os resultados estão na Tabela 13.

**Tabela 13 - Roteiro de testes funcionais no simulador (após as correções)**

| # | Ação | Resultado esperado | Resultado obtido |
|---|---|---|---|
| 1 | Energização da placa | Servo de 90° para Normal e travamento em cerca de 1,6 s | Travamento em Normal após 1.601 ms |
| 2 | Pedidos de Reversa e de Normal | Sinal vermelho, manobra de cerca de 3,1 s e travamento | Motor comandado em 367 µs; manobras de 3.110 ms e 3.108 ms |
| 3 | Pedido de Reversa com a via ocupada | Pedido em espera ("ocupado"); manobra automática ao liberar a via | Pedido em espera; executado sozinho 3.050 ms depois, ao liberar a via |
| 4 | Obstrução com o AMV travado | Falha por perda de detecção e sinal vermelho | Falha por perda de detecção com latência de 253 µs |
| 5 | Rearme com a obstrução mantida | Nova manobra e falha por timeout após 4.500 ms | Falha por timeout aos 4.500 ms, com diagnóstico "ang=13500 planta=- obs=1" |
| 6 | Retirada da obstrução e rearme | Novo travamento | Travamento imediato, pois o servo já estava em fim de curso |
| 7 | Emergência durante uma manobra, seguida de rearme | Sinal vermelho, parada do motor e registro da latência | Emergência com latência de 200 µs; após o rearme, manobra concluída em 1.548 ms |
| 8 | Trem a 40 km/h e pedidos alternados | Manobras com o trem distante; pedido em espera com o trem próximo | Duas manobras concluídas; o terceiro pedido ficou em espera com o circuito ocupado |
| 9 | Trem a 80 km/h e pedidos a cada 5 s | Pedidos em espera (travamento de aproximação) e trem no ramal a até 30 km/h | Pedidos guardados e executados após até 13.050 ms de espera; trem entrou no ramal desviado a 29 km/h |
| 10 | Obstrução durante uma manobra com o trem a 80 km/h | Falha por timeout e trem retido no sinal vermelho | Falha por timeout aos 4.500 ms; nenhum registro de ultrapassagem do vermelho ou de descarrilamento |

*Fonte - Próprio Autor (2026).*

Todos os passos tiveram o resultado esperado. As manobras voltaram a ser confirmadas pela interrupção, agora tratada pelo gancho de tick, e a duração caiu de 3.150 ms para 3.102 a 3.110 ms, coincidindo com os testes automáticos e com o cálculo da seção 3.1.2. As latências medidas pela própria placa, comparadas às metas e às estimativas da seção 3.1, são apresentadas na Tabela 14.

**Tabela 14 - Tempos calculados e medidos no simulador (após as correções)**

| Caminho | Meta | Calculado | Medido (último / máximo) | Atende |
|---|---|---|---|---|
| Emergência → sinal vermelho | ≤ 10 ms | ≈ 20 µs (pior caso ≈ 40 µs) | 200 / 200 µs (comando pela serial) | Sim |
| Perda de detecção → sinal vermelho | ≤ 10 ms | ≈ 20 µs (pior caso ≈ 40 µs) | 253 / 253 µs (+ até 1 ms até o tick) | Sim |
| Pedido → motor comandado | ≤ 50 ms | < 1 ms | 365 / 368 µs | Sim |
| Manobra completa | ≤ 4.500 ms | 3.100 a 3.120 ms | 3.102 a 3.110 ms | Sim |
| Jitter do período do motor | ≤ 1 ms | dezenas de µs | máximo de 555 µs | Sim |

*Fonte - Próprio Autor (2026).*

A latência da perda de detecção, de 253 µs, é medida a partir do instante em que o evento é tratado pelo gancho de tick; somando a espera de até 1 ms pelo próximo tick, o pior caso fica em torno de 1,25 ms, cerca de oito vezes abaixo da meta de 10 ms. A latência da emergência, de 200 µs, ficou 50 vezes abaixo da meta; ela foi acionada pelo comando serial, cujo caminho inclui a recepção e a interpretação do comando pela tarefa de comunicação, de menor prioridade, e por isso difere da estimativa da Tabela 5, que considera a interrupção do botão. Pelo mesmo motivo, a latência entre o pedido e o comando do motor, de 365 a 368 µs, permaneceu mais de 130 vezes abaixo da meta de 50 ms. A margem da manobra mais longa, de 3.110 ms, até o watchdog foi de 1.390 ms (31 %). O jitter do motor atendeu à tolerância de 1 ms, mas ficou uma ordem de grandeza acima do previsto pela análise de escalonabilidade e variou entre execuções (de 419 µs a 903 µs ao longo das execuções do roteiro), o que deixa pouca folga em relação ao limite. Os valores em microssegundos, de modo geral, ficaram acima das estimativas teóricas, o que pode estar relacionado ao custo da emulação do processador pelo simulador.

**Figura 6 - Painel web durante a execução do roteiro de testes**

[PREENCHER: print do painel com a tabela "medido × calculado" visível]

*Fonte - Próprio Autor (2026).*

### 5.3 USO DE MEMÓRIA

A pilha livre de cada tarefa, consultada pelo comando `stats` ao final do roteiro, está na Tabela 15. Esse valor corresponde à menor folga observada desde a inicialização (marca d'água do FreeRTOS) e permite verificar se o dimensionamento das pilhas é adequado.

**Tabela 15 - Pilha livre por tarefa ao final do roteiro**

| Tarefa | Pilha alocada (palavras) | Menor folga observada (palavras) | Folga |
|---|---|---|---|
| tSeg | 128 | 88 | 69 % |
| tMotor | 112 | 75 | 67 % |
| tInter | 128 | 77 | 60 % |
| tSinal | 96 | 62 | 65 % |
| tPlanta | 96 | 62 | 65 % |
| tTrem | 128 | 76 | 59 % |
| tComm | 384 (antes 256) | 133 (antes 8) | 35 % (antes 3 %) |

*Fonte - Próprio Autor (2026).*

Seis das sete tarefas mantiveram folga entre 59 % e 69 %, indicando que o dimensionamento é adequado. A tarefa de comunicação, que formata as linhas de telemetria com funções de formatação de texto e por isso consome mais pilha, passou de uma folga insuficiente de 8 palavras para 133 palavras após a ampliação.

Durante todo o roteiro, nem o gancho de estouro de pilha nem as asserções do kernel foram acionados, e a placa permaneceu respondendo aos comandos até o fim. O firmware final ocupa 29.120 bytes de Flash (text), 328 bytes de dados inicializados e 8.464 bytes de bss, dentro dos 32 KB de Flash e 12 KB de RAM do STM32C031C6.

### 5.4 DISCUSSÃO DOS RESULTADOS

Os testes confirmaram o comportamento previsto na modelagem: o sinal só foi liberado com o AMV travado e a detecção igual ao comando; pedidos recebidos com a via ocupada ou durante uma manobra foram guardados e executados automaticamente, sem que o AMV se movesse sob o trem; a perda de detecção, a obstrução durante a manobra e a emergência conduziram o sistema ao estado seguro, do qual ele só saiu por meio de rearme explícito; e, com o trem a 80 km/h, nenhuma ultrapassagem do sinal vermelho ou descarrilamento foi registrada. Todos os caminhos medidos atenderam às metas temporais com folga, e a duração da manobra correspondeu ao valor calculado. Esses resultados indicam que a divisão em tarefas com prioridades fixas, a comunicação por filas e o uso de mutex com herança de prioridade foram suficientes para atender às restrições do processo.

O resultado mais relevante da etapa de testes, no entanto, foi a falha encontrada na primeira rodada. Os testes automáticos no computador, por substituírem o hardware, não exercitavam as interrupções reais e não a revelaram; foi a execução no simulador que mostrou que a supervisão do AMV travado dependia de um único caminho. A correção, um segundo caminho verificado a cada tick, aplica a esse ponto o mesmo princípio de redundância já usado na confirmação da manobra. O episódio reforça a importância de validar o sistema em mais de um ambiente e de submeter os caminhos de segurança a testes de injeção de falha. Como trabalhos futuros, recomenda-se investigar a origem do jitter do motor, que, embora dentro da tolerância, superou a estimativa teórica.

Por fim, ressalta-se que todas as medições foram feitas em ambiente simulado. O Wokwi emula o núcleo Cortex-M0+ e os periféricos utilizados, mas, como observado, não reproduz fielmente todos eles, além de não representar efeitos elétricos como ruído e trepidação dos contatos, nem a dinâmica mecânica real de uma máquina de chave. Em uma implantação física, seria necessário repetir as medições na placa e substituir as tarefas de simulação (tPlanta e tTrem) pelos equipamentos de campo.

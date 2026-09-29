#!/usr/bin/env bash
# Testes do controlador de AMV no PC: compila as tarefas reais com o
# FreeRTOS (port POSIX) e roda todos os cenarios em paralelo, conferindo as
# linhas que a "placa" manda pela serial.
#
#   tests/run.sh              todos os cenarios (~80 s, em paralelo)
#   tests/run.sh fila cliques so' os escolhidos
#
# Logs completos em tests/out/<cenario>.log. So' Linux/macOS (pthreads).

set -uo pipefail
cd "$(dirname "$0")/.."

K=Middlewares/Third_Party/FreeRTOS/Source
POSIX=$K/portable/ThirdParty/GCC/Posix
OUT=tests/out
mkdir -p "$OUT"

echo "== Compilando (gcc + FreeRTOS POSIX) =="
gcc -O0 -g -pthread -Wall -DSTK_ESCALA=32 \
    -Itests/host -ICore/Inc -I$K/include -I$POSIX -I$POSIX/utils \
    -o $OUT/hil tests/cenarios.c tests/host/hw_host.c \
    Core/Src/amv_tasks.c Core/Src/amv_comm.c Core/Src/amv_trem.c \
    $K/tasks.c $K/queue.c $K/list.c $POSIX/port.c $POSIX/utils/wait_for_event.c \
  || { echo "erro: compilacao falhou" >&2; exit 1; }

# Expectativas: "cenario|deve ter|nao pode ter". "Deve ter" e' uma lista de
# regex separadas por ';', e "N*regex" exige pelo menos N ocorrencias (o boot
# ja' gera um "travado pos=N", por exemplo). "Nao pode ter" pode ficar vazio.
ESPERADO=(
  "manobras|3*travado pos=R;3*travado pos=N|falha tipo="
  "trem80|3*travado pos=R;pedido alvo=. origem=fila|falha tipo="
  "cliques|pendente alvo=N motivo=em_movimento;pendente alvo=R motivo=em_movimento;2*cancelado=novo_pedido;3*travado pos=N;2*travado pos=R|falha tipo="
  "emergencia|emergencia lat_us=;rearme;2*travado pos=R;2*travado pos=N|falha tipo="
  "fila|pendente alvo=R motivo=ocupado;pedido alvo=R origem=fila;travado pos=R|falha tipo="
  "obstrucao|falha tipo=perda_det;falha tipo=timeout .* obs=1;2*travado pos=N|"
  "fio_quebrado|falha tipo=timeout .* planta=R pinos=- obs=0|travado pos=R"
)

selecionados=("$@")
roda() { [ ${#selecionados[@]} -eq 0 ] || [[ " ${selecionados[*]} " == *" $1 "* ]]; }

echo "== Rodando cenarios (tempo real) =="
for linha in "${ESPERADO[@]}"; do
  nome=${linha%%|*}
  roda "$nome" && { timeout 150 $OUT/hil "$nome" > "$OUT/$nome.log" 2>&1 & }
done
wait

falhou=0
printf "\n%-14s %-9s %s\n" "cenario" "resultado" "manobras / latencia pedido->motor (ult/max us) / manobra (ult/max ms)"
for linha in "${ESPERADO[@]}"; do
  IFS='|' read -r nome deve nao <<< "$linha"
  roda "$nome" || continue
  log="$OUT/$nome.log"
  motivo=""
  grep -q '^FIM' "$log" || motivo="nao terminou"
  IFS=';' read -ra padroes <<< "$deve"
  for p in "${padroes[@]}"; do
    min=1
    if [[ $p =~ ^([0-9]+)\*(.*)$ ]]; then min=${BASH_REMATCH[1]}; p=${BASH_REMATCH[2]}; fi
    n=$(grep -Ec "$p" "$log")
    [ "$n" -ge "$min" ] || motivo="${motivo:+$motivo; }faltou: $p (${n}/${min})"
  done
  [ -n "$nao" ] && grep -Eq "$nao" "$log" && motivo="${motivo:+$motivo; }apareceu: $nao"
  tm=$(grep '^tm ' "$log" | tail -1)
  resumo=$(sed -E 's/.*lat_cmd=([0-9/]+) manobra=([0-9/]+).* n=([0-9]+).*/\3 \/ \1 \/ \2/' <<< "$tm")
  if [ -z "$motivo" ]; then
    printf "%-14s %-9s %s\n" "$nome" "ok" "$resumo"
  else
    printf "%-14s %-9s %s\n" "$nome" "FALHOU" "$motivo"
    falhou=1
  fi
done
exit $falhou

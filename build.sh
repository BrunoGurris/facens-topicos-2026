#!/usr/bin/env bash
# Compila o firmware e sobe o painel web (tools/plot_timer.py).
#
#   ./build.sh          compila e abre o painel
#   ./build.sh build    so compila
#   ./build.sh clean    make clean
#
# O simulador em si e' iniciado pelo VS Code (F1 -> Wokwi: Start Simulator);
# o painel fica esperando e conecta sozinho assim que ele subir.

set -euo pipefail
cd "$(dirname "$0")"

need() {
  command -v "$1" >/dev/null 2>&1 || { echo "erro: '$1' nao encontrado. $2" >&2; exit 1; }
}

need arm-none-eabi-gcc "Instale o toolchain (README, secao 1)."
need make              "Instale o GNU Make (README, secao 1)."

case "${1:-run}" in
  clean)
    make clean
    exit 0
    ;;
  build|run)
    echo "== Compilando firmware =="
    make -j 2>&1 | grep -v "_read\|_write\|_lseek\|_close\|warning: _" || true
    [ -f build/debug/build/blink.hex ] || { echo "erro: build falhou" >&2; exit 1; }
    arm-none-eabi-size build/debug/build/blink.elf
    ;;
  *)
    echo "uso: $0 [run|build|clean]" >&2
    exit 1
    ;;
esac

[ "${1:-run}" = "run" ] || exit 0

PY=python3; command -v python3 >/dev/null 2>&1 || PY=python
need "$PY" "Instale o Python 3."
"$PY" -c "import serial" 2>/dev/null || {
  echo "erro: pyserial nao encontrado. Instale com:" >&2
  echo "  sudo apt install python3-serial    (Linux)" >&2
  echo "  pip install pyserial               (Windows / venv)" >&2
  exit 1
}

# Porta do painel: padrao 8765, ou o que vier em --http N / --http=N
HTTP_PORT=8765
ARGS=("${@:2}")
for ((i = 0; i < ${#ARGS[@]}; i++)); do
  case "${ARGS[$i]}" in
    --http)   HTTP_PORT="${ARGS[$((i + 1))]:-$HTTP_PORT}" ;;
    --http=*) HTTP_PORT="${ARGS[$i]#--http=}" ;;
  esac
done

echo
echo "== Painel web =="
echo "Frontend:  http://localhost:$HTTP_PORT"
echo "Serial:    rfc2217://localhost:4000 (sobe junto com o simulador)"
echo
echo "Agora inicie o simulador no VS Code: F1 -> Wokwi: Start Simulator"
echo "(o painel conecta sozinho quando ele subir; Ctrl+C encerra)"
echo
exec "$PY" tools/plot_timer.py "${ARGS[@]}"

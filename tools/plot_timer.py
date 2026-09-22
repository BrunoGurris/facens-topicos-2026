#!/usr/bin/env python3
"""
Grafico em tempo real do timer da placa (HAL_GetTick) e painel de controle,
tudo pela serial do Wokwi.

Como funciona:
  1. O firmware imprime "tick=<ms> led=<0|1> blink=<0|1> period=<ms>" a cada
     piscada e aceita comandos de texto na RX (led on, blink off, period 250...).
  2. O Wokwi expoe a UART simulada num servidor RFC2217 (porta 4000, ver wokwi.toml).
  3. Este script le/escreve na serial via pyserial e serve uma pagina HTML em
     http://localhost:8765: os graficos chegam por Server-Sent Events e os
     botoes da pagina fazem POST /cmd, que vira uma linha escrita na serial.

Uso:
  make && (F1 -> Wokwi: Start Simulator)
  python3 tools/plot_timer.py            # abre o navegador sozinho
  python3 tools/plot_timer.py --no-open  # so imprime a URL

Depende apenas de pyserial (pip install pyserial / apt install python3-serial).
"""

import argparse
import json
import queue
import re
import sys
import threading
import time
import webbrowser
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("pyserial nao encontrado: pip install pyserial  (ou apt install python3-serial)")

LINE_RE = re.compile(rb"tick=(\d+)\s+led=([01])\s+blink=([01])\s+period=(\d+)")
TOOLS_DIR = Path(__file__).parent
HTML = (TOOLS_DIR / "plot_timer.html").read_text(encoding="utf-8")
CHARTJS = (TOOLS_DIR / "chart.umd.min.js").read_bytes()

HISTORY = 600          # amostras guardadas para quem abrir a pagina depois
history = deque(maxlen=HISTORY)
logs = deque(maxlen=50)   # ultimas mensagens nao-numericas da serial
subscribers = set()    # filas SSE de cada navegador conectado
lock = threading.Lock()
write_lock = threading.Lock()
status = {"connected": False, "error": ""}
port = None            # porta serial aberta (None enquanto o Wokwi nao sobe)


def broadcast(event):
    with lock:
        if event["type"] == "sample":
            history.append(event)
        elif event["type"] == "log":
            logs.append(event["text"])
        for q in list(subscribers):
            q.put(event)


def serial_reader(url, baud):
    """Reconecta pra sempre: o Wokwi so abre a porta depois do 'Start Simulator'."""
    global port
    prev_tick = None
    t0 = None
    while True:
        try:
            with serial.serial_for_url(url, baudrate=baud, timeout=1) as ser:
                port = ser
                status.update(connected=True, error="")
                broadcast({"type": "status", **status})
                print(f"[serial] conectado em {url}", flush=True)
                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    m = LINE_RE.search(raw)
                    if not m:
                        # Outras mensagens (ex.: "Botao pressionado!") viram eventos de log
                        broadcast({"type": "log", "text": raw.decode(errors="replace").strip()})
                        continue
                    tick, led, blink, period = (int(g) for g in m.groups())
                    now = time.monotonic()
                    if t0 is None or (prev_tick is not None and tick < prev_tick):
                        t0, prev_tick = now, None      # placa reiniciou
                    broadcast({
                        "type": "sample",
                        "host_ms": round((now - t0) * 1000),   # relogio do PC
                        "tick": tick,                          # relogio da placa
                        "dt": None if prev_tick is None else tick - prev_tick,
                        "led": led,
                        "blink": blink,
                        "period": period,
                    })
                    prev_tick = tick
        except Exception as e:  # noqa: BLE001 - qualquer falha: avisa e tenta de novo
            port = None
            if status["connected"] or status["error"] != str(e):
                status.update(connected=False, error=str(e))
                broadcast({"type": "status", **status})
                print(f"[serial] sem conexao ({e}); tentando de novo...", flush=True)
            time.sleep(1)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):  # silencia o log por request
        pass

    def do_GET(self):
        if self.path == "/events":
            return self.sse()
        if self.path == "/":
            return self.reply(200, HTML.encode(), "text/html; charset=utf-8")
        if self.path == "/chart.umd.min.js":
            return self.reply(200, CHARTJS, "application/javascript")
        self.send_error(404)

    def do_POST(self):
        """POST /cmd  {"cmd": "led on"}  ->  escreve "led on\n" na serial."""
        if self.path != "/cmd":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        try:
            cmd = json.loads(self.rfile.read(length)).get("cmd", "").strip()
        except (ValueError, AttributeError):
            cmd = ""
        if not cmd or "\n" in cmd or "\r" in cmd:
            return self.reply(400, b'{"error": "comando invalido"}')
        ser = port
        if ser is None:
            return self.reply(503, b'{"error": "placa nao conectada"}')
        try:
            with write_lock:
                ser.write((cmd + "\n").encode())
                ser.flush()
        except Exception as e:  # noqa: BLE001
            return self.reply(503, json.dumps({"error": str(e)}).encode())
        print(f"[cmd] {cmd}")
        broadcast({"type": "log", "text": f"> {cmd}"})
        self.reply(200, b'{"ok": true}')

    def reply(self, code, body, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        q = queue.Queue()
        with lock:
            snapshot, log_snapshot = list(history), list(logs)
            subscribers.add(q)
        try:
            self.send_event({"type": "status", **status})
            self.send_event({"type": "history", "samples": snapshot, "logs": log_snapshot})
            while True:
                try:
                    self.send_event(q.get(timeout=15))
                except queue.Empty:
                    self.wfile.write(b": ping\n\n")  # mantem a conexao viva
                    self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with lock:
                subscribers.discard(q)

    def send_event(self, event):
        self.wfile.write(f"data: {json.dumps(event)}\n\n".encode())
        self.wfile.flush()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--serial", default="rfc2217://localhost:4000",
                    help="URL pyserial da porta (padrao: rfc2217://localhost:4000, o Wokwi). "
                         "Para placa fisica use ex. /dev/ttyACM0 ou COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http", type=int, default=8765, help="porta da pagina web (padrao 8765)")
    ap.add_argument("--no-open", action="store_true", help="nao abrir o navegador automaticamente")
    args = ap.parse_args()

    threading.Thread(target=serial_reader, args=(args.serial, args.baud), daemon=True).start()

    url = f"http://localhost:{args.http}"
    server = ThreadingHTTPServer(("127.0.0.1", args.http), Handler)
    server.daemon_threads = True
    print(f"[web] grafico e controle em {url}  (Ctrl+C para sair)", flush=True)
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

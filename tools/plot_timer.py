#!/usr/bin/env python3
"""
Grafico em tempo real do timer da placa (HAL_GetTick) lido pela serial do Wokwi.

Como funciona:
  1. O firmware imprime "tick=<ms> led=<0|1>" a cada ciclo do loop.
  2. O Wokwi expoe a UART simulada num servidor RFC2217 (porta 4000, ver wokwi.toml).
  3. Este script le a serial via pyserial e serve uma pagina HTML em
     http://localhost:8765 que desenha os graficos ao vivo (Server-Sent Events).

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

LINE_RE = re.compile(rb"tick=(\d+)\s+led=([01])")
HTML = (Path(__file__).parent / "plot_timer.html").read_text(encoding="utf-8")

HISTORY = 600          # amostras guardadas para quem abrir a pagina depois
history = deque(maxlen=HISTORY)
logs = deque(maxlen=50)   # ultimas mensagens nao-numericas da serial
subscribers = set()    # filas SSE de cada navegador conectado
lock = threading.Lock()
status = {"connected": False, "error": ""}


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
    prev_tick = None
    t0 = None
    while True:
        try:
            with serial.serial_for_url(url, baudrate=baud, timeout=1) as port:
                status.update(connected=True, error="")
                broadcast({"type": "status", **status})
                print(f"[serial] conectado em {url}")
                while True:
                    raw = port.readline()
                    if not raw:
                        continue
                    m = LINE_RE.search(raw)
                    if not m:
                        # Outras mensagens (ex.: "Botao pressionado!") viram eventos de log
                        broadcast({"type": "log", "text": raw.decode(errors="replace").strip()})
                        continue
                    tick, led = int(m.group(1)), int(m.group(2))
                    now = time.monotonic()
                    if t0 is None or (prev_tick is not None and tick < prev_tick):
                        t0, prev_tick = now, None      # placa reiniciou
                    broadcast({
                        "type": "sample",
                        "host_ms": round((now - t0) * 1000),   # relogio do PC
                        "tick": tick,                          # relogio da placa
                        "dt": None if prev_tick is None else tick - prev_tick,
                        "led": led,
                    })
                    prev_tick = tick
        except Exception as e:  # noqa: BLE001 - qualquer falha: avisa e tenta de novo
            if status["connected"] or status["error"] != str(e):
                status.update(connected=False, error=str(e))
                broadcast({"type": "status", **status})
                print(f"[serial] sem conexao ({e}); tentando de novo...")
            time.sleep(1)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):  # silencia o log por request
        pass

    def do_GET(self):
        if self.path == "/events":
            return self.sse()
        if self.path != "/":
            self.send_error(404)
            return
        body = HTML.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
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
    print(f"[web] grafico em {url}  (Ctrl+C para sair)")
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

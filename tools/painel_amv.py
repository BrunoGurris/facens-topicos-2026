#!/usr/bin/env python3
"""
Painel web do controlador de AMV (desvio ferroviario), tudo pela serial do Wokwi.

Como funciona:
  1. O firmware manda linhas de texto pela USART2:
       st ...  estado do AMV a cada 250 ms (posicao, sinal, angulo do motor...)
       ev ...  eventos (pedido, travado, falha, emergencia, rearme, rejeitado)
       tm ...  tempos medidos: latencias em us e duracao das manobras em ms
       stk ... pilha livre de cada tarefa (resposta ao comando "stats")
     e aceita comandos de texto na RX (n, r, t, emg, rearme, occ 1, obs 1...).
  2. O Wokwi expoe a UART simulada num servidor RFC2217 (porta 4000, ver wokwi.toml).
  3. Este script le/escreve na serial via pyserial e serve uma pagina HTML em
     http://localhost:8765: os dados chegam por Server-Sent Events e os
     botoes da pagina fazem POST /cmd, que vira uma linha escrita na serial.

Uso:
  make && (F1 -> Wokwi: Start Simulator)
  python3 tools/painel_amv.py            # abre o navegador sozinho
  python3 tools/painel_amv.py --no-open  # so imprime a URL

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

KV_RE = re.compile(r"(\w+)=(\S+)")
TOOLS_DIR = Path(__file__).parent
HTML = (TOOLS_DIR / "painel_amv.html").read_text(encoding="utf-8")
CHARTJS = (TOOLS_DIR / "chart.umd.min.js").read_bytes()

HISTORY = 600          # amostras "st" guardadas (~150 s a 250 ms)
history = deque(maxlen=HISTORY)
logs = deque(maxlen=100)  # eventos e mensagens de texto
last = {}              # ultimo "tm" e "stk", para quem abrir a pagina depois
subscribers = set()    # filas SSE de cada navegador conectado
lock = threading.Lock()
write_lock = threading.Lock()
status = {"connected": False, "error": ""}
port = None            # porta serial aberta (None enquanto o Wokwi nao sobe)


def parse_value(v):
    """'1234' -> 1234, '10/250' -> [10, 250], resto fica texto."""
    if v.isdigit():
        return int(v)
    if "/" in v:
        a, _, b = v.partition("/")
        if a.isdigit() and b.isdigit():
            return [int(a), int(b)]
    return v


def parse_line(text):
    """Transforma 'st t=10 est=travado ...' em {'kind': 'st', 't': 10, 'est': 'travado'}."""
    kind, _, rest = text.partition(" ")
    if kind not in ("st", "ev", "tm", "stk"):
        return None
    data = {"kind": kind}
    if kind == "ev":
        # "ev t=123 travado pos=N ..." -> o nome do evento e' a 1a palavra sem '='
        words = rest.split()
        data["name"] = next((w for w in words if "=" not in w), "?")
    data.update({k: parse_value(v) for k, v in KV_RE.findall(rest)})
    return data


def broadcast(event):
    with lock:
        if event["type"] == "st":
            history.append(event)
        elif event["type"] in ("tm", "stk"):
            last[event["type"]] = event
        elif event["type"] in ("ev", "log"):
            logs.append(event)
        for q in list(subscribers):
            q.put(event)


def serial_reader(url, baud):
    """Reconecta pra sempre: o Wokwi so abre a porta depois do 'Start Simulator'."""
    global port
    t0 = None
    prev_t = None
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
                    text = raw.decode(errors="replace").strip()
                    if not text:
                        continue
                    data = parse_line(text)
                    if data is None:
                        broadcast({"type": "log", "text": text})
                        continue
                    now = time.monotonic()
                    if data["kind"] == "st":
                        t = data.get("t", 0)
                        if t0 is None or (prev_t is not None and t < prev_t):
                            t0 = now            # placa reiniciou
                        prev_t = t
                    data["type"] = data.pop("kind")
                    data["host_ms"] = round((now - (t0 or now)) * 1000)
                    data["text"] = text
                    broadcast(data)
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
        """POST /cmd  {"cmd": "r"}  ->  escreve "r\\n" na serial."""
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
            snapshot = {"type": "history", "samples": list(history),
                        "logs": list(logs), "last": dict(last)}
            subscribers.add(q)
        try:
            self.send_event({"type": "status", **status})
            self.send_event(snapshot)
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
    print(f"[web] painel do AMV em {url}  (Ctrl+C para sair)", flush=True)
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Servidor sencillo del semáforo (Arduino UNO por USB).

Recibe la señal de la placa por Serial USB y sirve la página web que refleja
el puerto/luz activo con cuenta regresiva.

Placa principal: Arduino UNO con `semaforo_arduino_uno.ino` (solo USB, sin WiFi).
Respaldo: ESP32 con `semaforo_esp32.ino` (también por Serial USB; su POST WiFi
sigue aceptado en /api/update por compatibilidad).

Uso:
    python server.py                      # intenta Serial, si no hay hardware usa modo demo
    python server.py --port COM3          # Windows, puerto donde está el Arduino UNO
    python server.py --port /dev/ttyUSB0  # Linux
    python server.py --sim                # forzar modo simulado (sin placa)
    python server.py --no-sim             # no simular aunque no haya Serial

Requiere (solo para leer la placa por USB):
    pip install pyserial

Sin pyserial igual funciona: modo simulado (y POST WiFi del ESP32 de respaldo).
Página: http://localhost:8000
API:    GET  /api/status  -> {"light","pin","remaining","durations","connected","source","server_time"}
        POST /api/update  <- {"light":"red|yellow|green","pin":2,"remaining":12}
"""
import argparse
import json
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent

VALID_LIGHTS = ("red", "yellow", "green")
DEFAULT_DURATIONS = {"green": 15, "yellow": 5, "red": 20}
DEFAULT_PINS = {"green": 5, "yellow": 4, "red": 2}

state_lock = threading.Lock()
state = {
    "light": "green",
    "pin": DEFAULT_PINS["green"],
    "remaining": DEFAULT_DURATIONS["green"],
    "durations": dict(DEFAULT_DURATIONS),
    "last_update": time.time(),
    "connected": False,
    "source": "ninguna",  # serial | wifi | simulado
}


def update_state(light, remaining=None, pin=None, durations=None, source="wifi"):
    if light not in VALID_LIGHTS:
        return False
    with state_lock:
        state["light"] = light
        try:
            state["pin"] = int(pin) if pin is not None else DEFAULT_PINS[light]
        except (TypeError, ValueError):
            state["pin"] = DEFAULT_PINS[light]
        if durations:
            for k in DEFAULT_DURATIONS:
                if k in durations:
                    try:
                        state["durations"][k] = int(durations[k])
                    except (TypeError, ValueError):
                        pass
        total = state["durations"][light]
        if remaining is None:
            remaining = total
        try:
            remaining = int(remaining)
        except (TypeError, ValueError):
            remaining = total
        state["remaining"] = max(0, min(remaining, total))
        state["last_update"] = time.time()
        state["connected"] = True
        state["source"] = source
    return True


def mark_disconnected_if_stale(timeout=4.0):
    with state_lock:
        if state["source"] in ("serial", "wifi") and time.time() - state["last_update"] > timeout:
            state["connected"] = False


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BASE_DIR), **kwargs)

    def log_message(self, *args):
        pass  # salida limpia

    def _send_json(self, obj, code=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.split("?")[0] == "/api/status":
            mark_disconnected_if_stale()
            with state_lock:
                payload = {
                    "light": state["light"],
                    "pin": state["pin"],
                    "remaining": state["remaining"],
                    "durations": dict(state["durations"]),
                    "connected": state["connected"],
                    "source": state["source"],
                    "server_time": time.time(),
                    "last_update": state["last_update"],
                }
            self._send_json(payload)
        else:
            super().do_GET()

    def do_POST(self):
        if self.path.split("?")[0] == "/api/update":
            try:
                length = int(self.headers.get("Content-Length", 0))
            except ValueError:
                length = 0
            try:
                data = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
            except (json.JSONDecodeError, UnicodeDecodeError):
                return self._send_json({"ok": False, "error": "JSON inválido"}, 400)
            ok = update_state(
                data.get("light"),
                remaining=data.get("remaining"),
                pin=data.get("pin"),
                durations=data.get("durations"),
                source="wifi",
            )
            return self._send_json({"ok": ok}, 200 if ok else 400)
        return self._send_json({"ok": False, "error": "ruta no encontrada"}, 404)


def serial_reader(port, baud):
    """Lee líneas JSON de la placa (Arduino UNO) por USB y actualiza el estado."""
    try:
        import serial  # pyright: ignore[reportMissingImports]
    except ImportError:
        print("[serial] pyserial no instalado -> omite USB. Instala con: pip install pyserial")
        return
    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"[serial] no se pudo abrir {port}: {e}")
        return
    print(f"[serial] escuchando placa en {port} @ {baud}")
    buf = b""
    while True:
        try:
            chunk = ser.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if not line:
                    continue
                try:
                    data = json.loads(line.decode("utf-8", errors="ignore"))
                except json.JSONDecodeError:
                    continue
                if isinstance(data, dict) and data.get("light") in VALID_LIGHTS:
                    update_state(
                        data["light"],
                        remaining=data.get("remaining"),
                        pin=data.get("pin"),
                        durations=data.get("durations"),
                        source="serial",
                    )
        except Exception as e:
            print(f"[serial] error: {e}")
            time.sleep(2)


def simulator():
    """Simula el ciclo del semáforo cuando no hay placa (verde 15 -> amarilla 5 -> roja 20)."""
    order = ["green", "yellow", "red"]
    print("[sim] modo simulado activado (sin placa conectada)")
    while True:
        for light in order:
            total = DEFAULT_DURATIONS[light]
            for remaining in range(total, -1, -1):
                update_state(light, remaining=remaining, pin=DEFAULT_PINS[light], source="simulado")
                time.sleep(1)


def main():
    ap = argparse.ArgumentParser(description="Servidor del semáforo (Arduino UNO por USB)")
    ap.add_argument("--port", default=None, help="Puerto serial del Arduino UNO (ej. COM3). Si se omite, se intenta autodetectar.")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8000)
    ap.add_argument("--sim", action="store_true", help="Forzar modo simulado")
    ap.add_argument("--no-sim", action="store_true", help="No simular aunque no haya Serial")
    args = ap.parse_args()

    port = args.port
    if port is None and not args.sim:
        # Autodetección básica del puerto del Arduino UNO / ESP32 de respaldo
        try:
            from serial.tools import list_ports  # pyright: ignore[reportMissingImports]
            for p in list_ports.comports():
                desc = f"{p.description} {p.manufacturer or ''}".lower()
                if any(k in desc for k in ("arduino", "uno", "ch340", "ch341", "cp210", "ftdi", "silabs", "usb serial", "esp32")):
                    port = p.device
                    break
        except ImportError:
            pass

    threads = []
    if args.sim or (port is None and not args.no_sim):
        t = threading.Thread(target=simulator, daemon=True)
        t.start()
        threads.append(t)
    elif port:
        t = threading.Thread(target=serial_reader, args=(port, args.baud), daemon=True)
        t.start()
        threads.append(t)

    srv = ThreadingHTTPServer(("0.0.0.0", args.http_port), Handler)
    print(f"[http] página en http://localhost:{args.http_port}")
    print(f"[http] API en   http://localhost:{args.http_port}/api/status")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n[http] servidor detenido")


if __name__ == "__main__":
    main()

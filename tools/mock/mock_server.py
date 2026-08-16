#!/usr/bin/env python3
"""Mock do firmware CriptoHost NerdOS — serve data/ + /api/* fake para validar a UI sem placa.

Uso:  python3 tools/mock/mock_server.py [porta]   (default 8091)
"""
import json, os, random, sys, time
from http.server import HTTPServer, SimpleHTTPRequestHandler

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "data")
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8091
T0 = time.time()

def status():
    up = int(time.time() - T0) + 33743
    acc = 241 + up % 50
    rej = 10
    return {
        "worker": "CH-DevKit-01", "ip": "192.168.1.66",
        "hardware": "ESP32 DevKit V1", "fw": "v0.1.0-alpha",
        "status": "mining", "hashrate_khs": 356.2 + random.uniform(-8, 8),
        "temp_c": 53.0 + random.uniform(-2, 2), "rssi_dbm": -52,
        "uptime_s": up, "pool": "eu.digi.hmpool.io:3337",
        "shares": {"found": 251, "sent": acc + rej + 1, "accepted": acc, "rejected": rej, "pending": 1},
        "best_difficulty": 7.1642, "templates": 2307, "valid_blocks": 0,
    }

EVENTS = [
    {"t": 33700, "type": "accept", "msg": "Share aceito pela pool"},
    {"t": 33650, "type": "share", "msg": "Share enviado #251"},
    {"t": 33500, "type": "reject", "msg": "Share rejeitado pela pool"},
    {"t": 33000, "type": "job", "msg": "Template #2300"},
    {"t": 30000, "type": "conn", "msg": "Minerando em eu.digi.hmpool.io"},
]

class H(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)

    def log_message(self, *a):  # silencioso
        pass

    def _json(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path == "/api/status": return self._json(status())
        if self.path == "/api/events": return self._json(EVENTS)
        if self.path == "/api/config":
            return self._json({"pool": "eu.digi.hmpool.io", "port": 3337,
                               "wallet": "DAbCdEf123.CH-DevKit-01", "password": "x",
                               "timezone": 2, "fw": "v0.1.0-alpha", "hardware": "ESP32 DevKit V1"})
        if self.path == "/api/fleet":
            return self._json({"self": status(), "peers": []})
        if self.path == "/api/bench":
            return self._json({"sw_khs": 78.4, "live_khs": 356.2, "method": "hw"})
        return super().do_GET()

    def do_POST(self):
        self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        return self._json({"ok": True})

if __name__ == "__main__":
    print(f"CriptoHost NerdOS mock — http://localhost:{PORT}")
    HTTPServer(("0.0.0.0", PORT), H).serve_forever()

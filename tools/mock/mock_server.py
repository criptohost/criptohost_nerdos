#!/usr/bin/env python3
"""Mock do firmware CriptoHost NerdOS — serve data/ + /api/* fake para validar a UI sem placa.

Uso:  python3 tools/mock/mock_server.py [porta]   (default 8091)
"""
import json, os, random, sys, time
from http.server import HTTPServer, SimpleHTTPRequestHandler

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "data")
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8091
WORKER = sys.argv[2] if len(sys.argv) > 2 else "CH-DevKit-01"
HW = sys.argv[3] if len(sys.argv) > 3 else "ESP32 DevKit V1"
BASE_KHS = float(sys.argv[4]) if len(sys.argv) > 4 else 356.2
# instância principal (8091) enxerga peers fake nas portas 8092/8093
PEERS = [{"worker": "CH-S3-02", "fw": "v0.1.0-alpha", "hardware": "ESP32-S3", "ip": "localhost", "port": 8092},
         {"worker": "CH-TDS3-03", "fw": "v0.1.0-alpha", "hardware": "LilyGO T-Display S3", "ip": "localhost", "port": 8093}] if PORT == 8091 else []
T0 = time.time()

def status():
    up = int(time.time() - T0) + 33743
    acc = 241 + up % 50
    rej = 10
    return {
        "worker": WORKER, "hostname": WORKER.lower().replace(".", "-"), "ip": "192.168.1.%d" % (60 + PORT % 10),
        "mac": "24:6F:28:00:%02X:%02X" % ((PORT // 256) % 256, PORT % 256),
        "hardware": HW, "fw": "v0.1.0-alpha",
        "status": "mining", "hashrate_khs": BASE_KHS + random.uniform(-8, 8),
        "temp_c": 53.0 + random.uniform(-2, 2), "rssi_dbm": -52,
        "uptime_s": up, "pool": "dgb.fusionpool.pro:3332",
        "shares": {"found": 251, "sent": acc + rej + 1, "accepted": acc, "rejected": rej, "pending": 1},
        "best_difficulty": 7.1642, "templates": 2307, "valid_blocks": 0,
    }

EVENTS = [
    {"t": 33700, "type": "accept", "msg": "Share accepted by pool"},
    {"t": 33650, "type": "share", "msg": "Share sent #251"},
    {"t": 33500, "type": "reject", "msg": "Share rejected — 23 Low difficulty share · miner: share below pool difficulty · diff 0.0012"},
    {"t": 33000, "type": "job", "msg": "Template #2300"},
    {"t": 30000, "type": "conn", "msg": "Mining on dgb.fusionpool.pro"},
]
ERRORS = [
    {"t": 33500, "type": "reject", "msg": "Share rejected — 23 Low difficulty share · miner: share below pool difficulty · diff 0.0012"},
    {"t": 28000, "type": "reject", "msg": "Share rejected — 23 Low difficulty share · miner: share below pool difficulty · diff 0.0011"},
    {"t": 12000, "type": "conn", "msg": "Stratum error — connection timeout"},
]

class H(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)

    def log_message(self, *a):  # silencioso
        pass

    def end_headers(self):  # dev: nunca cachear
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

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
        if self.path == "/api/errors": return self._json(ERRORS)
        if self.path == "/api/config":
            return self._json({"pool": "dgb.fusionpool.pro", "port": 3332,
                               "wallet": "DAbCdEf123." + WORKER, "password": "x",
                               "timezone": 2, "fw": "v0.1.0-alpha", "hardware": "ESP32 DevKit V1"})
        if self.path == "/api/fleet":
            return self._json({"self": status(), "peers": PEERS, "foreign": [{
                "worker": "bitaxe-supra", "hostname": "bitaxe", "ip": "192.168.1.90",
                "port": 80, "platform": "foreign", "vendor": "Bitaxe/AxeOS",
                "hardware": "BM1368", "fw": "v2.4.1",
                "status": "mining", "hashrate_khs": 625000000.0, "temp_c": 58.4,
                "rssi_dbm": -48, "uptime_s": 91234, "pool": "public-pool.io:21496",
                "mac": "5C:01:3B:AA:12:34", "coin": "BTC",
                "shares": {"found": 3421, "sent": 3421, "accepted": 3401,
                           "rejected": 20, "pending": 0},
                "best_difficulty": "4.29M", "templates": 0, "valid_blocks": 0}]})
        if self.path == "/api/peers":
            return self._json({"content": "192.168.1.205        # CH-8634\n192.168.1.62:8091    # mac\n", "editable": True})
        if self.path == "/api/wifi":
            return self._json({"ssid": "Casa", "rssi": -52, "ip": "192.168.1.66"})
        if self.path == "/api/wifi/scan":
            return self._json([
                {"ssid": "Casa", "rssi": -48, "open": False},
                {"ssid": "Vizinho", "rssi": -70, "open": False},
                {"ssid": "Guest", "rssi": -62, "open": True},
            ])
        if self.path == "/api/bench":
            return self._json({"sw_khs": 78.4, "live_khs": 356.2, "method": "hw"})
        return super().do_GET()

    def do_POST(self):
        self.rfile.read(int(self.headers.get("Content-Length", 0) or 0))
        return self._json({"ok": True})

if __name__ == "__main__":
    print(f"CriptoHost NerdOS mock — http://localhost:{PORT}")
    HTTPServer(("0.0.0.0", PORT), H).serve_forever()

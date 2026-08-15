# API — CriptoHost NerdOS

> EN summary: every node exposes the same HTTP+WS contract on port 80. This contract is frozen (golden rule): fleets mixing firmware generations must interoperate.

Base: `http://<ip-do-nó>` · portas: HTTP 80 · CORS liberado nas rotas GET (fleet cross-origin).

## GET /api/status — contrato fleet (§4.2 do escopo)

```json
{
  "worker": "CH-DevKit-01", "ip": "192.168.1.66",
  "hardware": "ESP32 DevKit V1", "fw": "v0.1.0-alpha",
  "status": "mining", "hashrate_khs": 356.2,
  "temp_c": 53.0, "rssi_dbm": -52, "uptime_s": 33743,
  "pool": "eu.digi.hmpool.io:3337",
  "shares": {"found":251,"sent":251,"accepted":241,"rejected":10,"pending":0},
  "best_difficulty": 7.1642, "templates": 2307, "valid_blocks": 0
}
```

`status`: `mining` | `idle` | `offline`.

## GET /api/events

Últimos 24 eventos (share stream + connection log): `[{"t":<uptime_s>,"type":"share|accept|reject|job|block|conn","msg":"..."}]`

## GET /api/config · POST /api/config

GET devolve `{pool, port, wallet, password, timezone, fw, hardware}`.
POST aceita o mesmo JSON (campos opcionais), valida (pool 4–128 chars, port 1–65535, wallet 8–79), persiste e **reinicia** (Save & Restart). Respostas: `200 {"ok":true,"restarting":true}` · `400 {"error":...}`.

## POST /api/identify

Pisca o LED do nó por 10 s (5 Hz). `200 {"ok":true}`.

## POST /api/restart · POST /api/factory-reset

Restart reinicia mantendo config. Factory reset apaga config + Wi-Fi e volta ao portal `CriptoHostAP` (a UI pede confirmação dupla).

## POST /api/ota

`multipart/form-data` com o `.bin`. Valida magic byte `0xE9` no primeiro chunk (binário inválido → `400`, sem brick). Sucesso → `200` e reboot. Config preservada (partições de app ≠ FS/NVS).

## GET /api/fleet

```json
{"self": {<status deste nó>}, "peers": [{"worker","fw","hardware","ip","port"}]}
```

Peers vêm de query mDNS `_criptohost._tcp` (cache; a primeira chamada dispara o refresh — repolle em ~5 s).

## GET /api/bench

`{"sw_khs": 123.4, "live_khs": 356.2, "method": "hw|sw"}` — benchmark SW síncrono (200 ms) + hashrate ao vivo.

## WS /ws

Push a cada 5 s: `{"status": {<contrato /api/status>}, "events": [<contrato /api/events>]}`.

## mDNS

Serviço `_criptohost._tcp` porta 80, TXT: `worker`, `fw`, `hardware`. Hostname = worker em minúsculas (`ch-devkit-01.local`).

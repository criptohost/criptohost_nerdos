# API — CriptoHost NerdOS

> EN summary: every node exposes the same HTTP+WS contract on port 80. This contract is frozen (golden rule): fleets mixing firmware generations must interoperate.

Base: `http://<ip-do-nó>` · portas: HTTP 80 · CORS liberado nas rotas GET (fleet cross-origin).

## GET /api/status — contrato fleet (§4.2 do escopo)

```json
{
  "worker": "CH-DevKit-01", "hostname": "ch-devkit-01-a3f2", "ip": "192.168.1.66",
  "mac": "24:6F:28:A3:F2:B1",
  "hardware": "ESP32 DevKit V1", "fw": "v0.1.0-alpha",
  "status": "mining", "hashrate_khs": 356.2,
  "temp_c": 53.0, "rssi_dbm": -52, "uptime_s": 33743,
  "pool": "digi.hmpool.io:3337",
  "shares": {"found":251,"sent":251,"accepted":241,"rejected":10,"pending":0},
  "best_difficulty": 7.1642, "templates": 2307, "valid_blocks": 0
}
```

`status`: `mining` | `connecting` | `idle` | `offline`.

## GET /api/events

Últimos 24 eventos (share stream + connection log): `[{"t":<uptime_s>,"type":"share|accept|reject|job|block|conn","msg":"..."}]`

## GET /api/errors

Ring próprio de rejects e falhas de conexão (até 48), independente do live log de 24 eventos. Mesmo shape de `/api/events`. Idle/checking de rotina **não** entram aqui.

## GET /api/config · POST /api/config

GET devolve `{pool, port, wallet, password, timezone, hostname, ap_ssid, fw, hardware}`.
POST aceita o mesmo JSON (campos opcionais), valida (pool 4–128 chars, port 1–65535, wallet 8–79), persiste e **reinicia** (Save & Restart). Respostas: `200 {"ok":true,"restarting":true}` · `400 {"error":...}`.

## GET /api/wifi · POST /api/wifi · GET /api/wifi/scan

Troca da rede STA sem voltar ao captive portal.

- GET `/api/wifi` → `{"ssid":"<atual>","rssi":-52,"ip":"192.168.1.66"}`.
- GET `/api/wifi/scan` dispara um scan no `loop()` e devolve o cache (repole em ~3 s) → `[{"ssid","rssi","open"}]`.
- POST `/api/wifi` `{ssid, password}` (ssid 1–32; password vazio = rede aberta, senão 8–63). Grava na NVS do Wi-Fi e **reinicia**. Se a nova rede falhar, o AP `CriptoHostNerdOS-XXXX` volta (XXXX = 4 hex do MAC).

## POST /api/restart · POST /api/factory-reset

Restart reinicia mantendo config. Factory reset apaga config + Wi-Fi e volta ao portal `CriptoHostNerdOS-XXXX` (a UI pede confirmação dupla).

## POST /api/ota

`multipart/form-data` com o `.bin`. Valida magic byte `0xE9` no primeiro chunk (binário inválido → `400`, sem brick). Sucesso → `200` e reboot. Config preservada (NVS/LittleFS fora das partições de app). Exige tabela com `ota_0` **e** `ota_1` (`partitions/ch_4mb_ota.csv`). Builds antigos em `huge_app.csv` (só `ota_0`) não têm slot OTA — `POST /api/ota/prepare` devolve 400; flash USB factory uma vez.

## GET /api/fleet

```json
{"self": {<status deste nó>}, "peers": [{"worker","fw","hardware","ip","port"}]}
```

Peers vêm de query mDNS `_criptohost._tcp` (cache; a primeira chamada dispara o refresh — repolle em ~5 s) somados às entradas da lista replicada (`/api/peers`), que podem trazer `"token"` — o browser envia esse valor em `X-CH-Token` ao consultar nós expostos na internet.

## GET /api/peers · POST /api/peers

Lista de peers replicada por gossip (frota inteira converge sozinha, revisão maior vence):

```json
{"content": "192.168.1.205\n1.2.3.4:8091 kJ8x…\n", "rev": 1788554496, "editable": true}
```

Formato: uma linha por nó, `ip[:porta] [token]` (porta default 80; `#` comenta; token = acesso a nó exposto). POST `{"content": "…"}` = edição humana (o nó cria uma revisão nova e ela propaga); POST `{"content": "…", "rev": N}` = push de sincronização (aceito só se `N` for maior que a revisão local; senão `{"ok":false,"stale":true}`). Cada nó consulta os vizinhos a cada ~60 s; agents também fazem push (alcança VPS que não conseguem puxar da rede de casa). Armazenado em `/peers.conf` + `/peers.rev` (LittleFS na placa, `ch/` no agent).

## GET /api/bench

`{"sw_khs": 123.4, "live_khs": 356.2, "method": "hw|sw"}` — benchmark SW síncrono (200 ms) + hashrate ao vivo.

## WS /ws

Push a cada 5 s: `{"status": {<contrato /api/status>}, "events": [<contrato /api/events>], "errors": [<contrato /api/errors>]}`.

## mDNS

Serviço `_criptohost._tcp` porta 80, TXT: `worker`, `fw`, `hardware`. Hostname DHCP/mDNS = worker sanitizado + sufixo MAC (`ch-01-a3f2.local`) para várias placas na mesma LAN.

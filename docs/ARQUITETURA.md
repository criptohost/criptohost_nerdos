# Arquitetura — CriptoHost NerdOS

> EN summary: fork of NerdMiner_v2 keeping its mining core intact; the CH layer (`src/ch/`, enabled by `-D CH_BUILD`) adds the local web dashboard, fleet API, mDNS discovery and browser OTA.

## Visão por nó

```
┌─────────────────────────────────────────────────────────┐
│                  CriptoHost NerdOS (nó)                  │
│  Miner Task (Core0)  Miner Task (Core1)  Stratum Task    │
│  HW SHA + midstate ──┴── nerdSHA256plus  subscribe/auth  │
│         └──────── globais de estado ─────── notify/submit│
│                        │                                 │
│   Web Server (ESPAsyncWebServer, src/ch/ch_web.cpp)      │
│    /            dashboard (LittleFS, tema CH)            │
│    /api/status  JSON de telemetria (contrato fleet)      │
│    /api/config  GET/POST configuração (NVS/LittleFS)     │
│    /api/restart /api/factory-reset                       │
│    /api/ota     upload .bin (Update.h)                   │
│    /api/fleet   peers mDNS · /api/bench · /api/events    │
│    /ws          WebSocket push (5 s)                     │
│   mDNS: _criptohost._tcp · config: LittleFS · UI: LittleFS│
└─────────────────────────────────────────────────────────┘
```

## Mapa de código

| Escopo (§5) | Neste repo | Origem |
|---|---|---|
| `mining/miner` | `src/mining.cpp` (`minerWorkerHw/Sw`) | upstream (intacto) |
| `mining/sha256_hw` | `src/ShaTests/nerdSHA_HWTest.*` + caminho HW do miner | upstream |
| `mining/sha256_sw` | `src/ShaTests/nerdSHA256plus.*` (midstate caching) | upstream |
| `mining/stratum` | `src/stratum.cpp` | upstream (intacto) |
| `net/wifi_mgr` | `src/wManager.cpp` (WiFiManager, AP `CriptoHostNerdOS`) | upstream + re-skin |
| `net/mdns_svc` | `src/ch/ch_mdns.cpp` | **novo** |
| `web/server` + `web/ws_push` + `web/ota` | `src/ch/ch_web.cpp` | **novo** |
| `state/monitor` | `src/ch/ch_state.cpp` (single source do contrato) | **novo** |
| `state/settings` | `src/drivers/storage/` (LittleFS em builds CH) | upstream + patch |
| `market/prices` | `data/app.js` (CoinGecko no browser, cache 5 min) | **novo** |
| `drivers/display` | `src/drivers/` | upstream |

Decisões:

- **Camada CH isolada**: todo código novo vive em `src/ch/` sob `#ifdef CH_BUILD`. Os 30+ envs upstream continuam compilando sem nenhuma mudança funcional (M0-01).
- **Sem servidor central**: qualquer nó abre `/fleet.html`; o próprio nó consulta mDNS (`/api/fleet`) e o browser busca `/api/status` de cada peer (CORS liberado nas rotas de leitura).
- **Preços no browser, não no ESP32**: evita TLS + heap no firmware (risco §8 do escopo); falha de fetch não toca a mineração.
- **Contadores de shares** (`ch_shares_sent/accepted/rejected`): únicas 3 variáveis adicionadas ao core, incrementos de 1 linha nos pontos de submit/accept/reject.
- **Contrato `/api/*` é invariante** — é a regra de ouro para a migração ESP-IDF (M4) e frotas mistas.

## Tasks e prioridades (herdadas do upstream)

| Task | Core | Prio | Papel |
|---|---|---|---|
| MinerHw-0 / MinerSw-1 | 0/1 | 3/1 | loop de nonce (HW SHA + midstate) |
| Stratum | 1 | 4 | subscribe/authorize/notify/submit |
| Monitor | 1 | 5 | hashrate, uptime, persistência de stats |
| loop() Arduino | 1 | 4 | WiFiManager + camada CH (web/ws/mdns) |

O servidor web é assíncrono (AsyncTCP) e o push de telemetria roda no `loop()` — impacto no hashrate fica no ruído (<2%, CA do M1-06); a query mDNS do fleet roda no `loop()` para nunca bloquear a task async.

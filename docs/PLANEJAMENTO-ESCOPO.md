# CriptoHost NerdOS — Planejamento e Escopo de Desenvolvimento

**Versão:** 2.1 · **Data:** 04/09/2026 · *(v2.1: metas de hashrate em dois níveis + pipeline SHA de registros diretos)*
**Produto de:** [Cripto Host](https://cripto.host) — "Node de um jeito fácil"
**Base de código:** fork de [BitMaker-hub/NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) (GPL-3.0)
**Referência de regras de negócio e escopo funcional:** Ha•Kou NerdOS (usado apenas como benchmark de funcionalidades; interface, identidade visual e marca serão 100% Cripto Host)
**Plano de evolução:** Arduino/PlatformIO (Fases 0–2) → migração incremental para ESP-IDF 6.0 (Fases 3–4)

---

## 1. Contexto e posicionamento

O **CriptoHost NerdOS** é um firmware open-source de mineração SHA-256 para ESP32, fornecido pela Cripto Host como produto de comunidade/educação, alinhado ao posicionamento da empresa ("Node de um jeito fácil" → agora também "Miner de um jeito fácil").

O Ha•Kou NerdOS serve **exclusivamente como referência de escopo funcional** (quais telas, quais funcionalidades, qual experiência). Nenhum asset, marca, mascote, texto ou código do Ha•Kou será utilizado. A base de código real é o NerdMiner_v2 (GPL-3.0), e toda a camada visual será construída com a identidade Cripto Host:

- **Tema:** dark com paleta derivada da marca Cripto Host (base #0B041A — roxo profundo do site oficial), tipografia e componentes próprios.
- **Naming:** dispositivos `CH-<modelo>-<nn>` (ex.: `CH-DevKit-01`), AP de provisionamento `CriptoHostNerdOS`, mDNS `_criptohost._tcp`.
- **Domínios/canais:** subdomínio sugerido `nerdos.cripto.host` (docs + web flasher), repositório `github.com/criptohost/criptohost-nerdos`.

**Enquadramento honesto (regra de negócio):** projeto de hobby, educação e comunidade (lottery mining + PPLNS). Retorno financeiro esperado ~zero; o valor para a Cripto Host é marca, comunidade e funil educacional para os produtos de node.

---

## 2. Objetivos mensuráveis

| # | Objetivo | Meta |
|---|---|---|
| O1 | Hashrate por placa — **baseline** (garante a v1.0) | ≥300 kH/s (S3) e ≥350 kH/s (DevKit V1) |
| O1b | Hashrate por placa — **stretch** (pipeline SHA de registros diretos) | ≥700 kH/s (DevKit V1); meta de pesquisa 1 MH/s |
| O2 | Dashboard web local com atualização ao vivo | Refresh ≤5 s, sem travar a mineração |
| O3 | Fleet management na LAN | Descoberta automática ≥95% dos nós em ≤30 s |
| O4 | OTA via web sem cabo USB | Sucesso ≥99%, config preservada, rollback em falha |
| O5 | Estabilidade | Uptime ≥7 dias sem reboot; eficiência de shares ≥95% |
| O6 | Open source | Repo público GPL-3.0, build reproduzível em 1 comando |
| O7 | Identidade | 100% dos assets visuais próprios da Cripto Host (zero reuso de terceiros) |

---

## 3. Escopo

### 3.1 Dentro do escopo (produto final)

1. **Mining core** SHA-256d com acelerador de hardware + midstate caching, Stratum V1 completo.
2. **Dashboard web** (Home): hashrate ao vivo, shares (found/sent/accepted/rejected/pending), eficiência, best difficulty, uptime, templates, temperatura, RSSI Wi-Fi, preços BTC/DGB/XEC, connection log.
3. **Fleet**: descoberta mDNS, cards por dispositivo (hashrate, temp, Wi-Fi, pool, versão, status), agregados (devices online, hashrate total, temp média), ações remotas: Config, Restart, OTA.
4. **Configuration**: pool address/port, wallet.worker, password/difficulty (`d=X`), Save & Restart via web.
5. **OTA Update**: upload de `.bin` via web, config preservada, barra de progresso, proteção contra desconexão.
6. **Provisionamento**: captive portal (`CriptoHostNerdOS`) no primeiro boot.
7. **Ações do dispositivo**: Restart, Factory Reset.
8. **Multi-moeda SHA-256d**: DGB (default, hmpool), BTC (pool.nerdminers.org / pool.nerdminer.io), XEC e BCH como perfis opcionais.
9. **Placas**: Tier 1 — ESP32 DevKit V1 e ESP32-S3 (com e sem display); Tier 2 — C3/C6; Tier 3 — P4+C6 (experimental).
10. **Identidade visual Cripto Host** em todas as telas, flasher e docs (O7).

### 3.2 Fora do escopo (v1)

Stratum V2 (Fase 4, opt-in), merged mining (NMC), algoritmos não-SHA256d, app mobile nativo (dashboard responsivo cobre), cloud/telemetria externa, modelo de licença/ativação paga.

---

## 4. Arquitetura de referência

### 4.1 Diagrama lógico (por nó)

```
┌─────────────────────────────────────────────────────────┐
│                  CriptoHost NerdOS (nó)                  │
│                                                          │
│  ┌───────────┐  ┌───────────┐  ┌──────────────────────┐ │
│  │ Miner Task │  │ Miner Task│  │  Stratum Task        │ │
│  │  (Core 0)  │  │  (Core 1) │◄─┤  subscribe/authorize │ │
│  │  HW SHA +  │  │  HW SHA + │  │  notify/submit       │ │
│  │  midstate  │  │  midstate │  │  suggest_difficulty  │ │
│  └─────┬──────┘  └─────┬─────┘  └──────────┬───────────┘ │
│        └───────┬───────┘                   │             │
│         ┌──────▼───────┐          ┌────────▼──────────┐  │
│         │ Monitor/State │◄────────┤  Network Task     │  │
│         └──────┬───────┘          └───────────────────┘  │
│   ┌────────────▼──────────────────────────────────────┐  │
│   │ Web Server (ESPAsyncWebServer)                     │  │
│   │  /            dashboard (LittleFS, tema CH)        │  │
│   │  /api/status  JSON de telemetria (fleet contract)  │  │
│   │  /api/config  GET/POST configuração                │  │
│   │  /api/restart /api/factory-reset                   │  │
│   │  /api/ota     upload .bin (Update.h)               │  │
│   │  /ws          WebSocket push (5 s)                 │  │
│   └────────────────────────────────────────────────────┘  │
│   mDNS: _criptohost._tcp · NVS: config · LittleFS: UI    │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Contrato de API (o coração do Fleet)

```json
GET /api/status →
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

Decisão de design: **sem servidor central** — qualquer nó abre o Fleet e enxerga os demais via mDNS (peer-to-peer). Futuro opcional: agregador Grafana/InfluxDB consumindo o mesmo endpoint.

### 4.3 Módulo de aceleração SHA-256 (diferencial de performance)

Dois níveis de implementação, ambos atrás da mesma interface `IShaBackend`:

**Nível 1 — Baseline (M1-01/02, ≥350 kH/s):** periférico SHA via mbedTLS com `CONFIG_MBEDTLS_HARDWARE_SHA` ou chamadas `esp_sha` do IDF; midstate caching; fallback automático para software (caminho do NerdMiner) → compatibilidade com S2 e portes futuros. É o patamar comprovado (BitMiner24 no S3, referência de escopo no DevKit) e o que garante a v1.0.0.

**Nível 2 — Pipeline de registros diretos (M1-13, stretch ≥700 kH/s no DevKit V1):** o mesmo silício dos miners comerciais de ~1000 kH/s (NMMiner em ESP32-WROOM-32) — a diferença é só firmware. Técnicas a implementar, na ordem de ganho esperado:
1. **Escrita direta nos registros do motor SHA** (sem mbedTLS, locks ou cópias de buffer) — referência open GPL: SparkMiner (~715–725 kH/s no ESP32 clássico) + Technical Reference Manual da Espressif.
2. **Pipelining CPU ↔ periférico**: enquanto o motor processa o bloco N, a CPU prepara o header N+1 (nonce + escrita de registros); o gargalo passa a ser o barramento, não o cálculo.
3. **Early-exit no teste do target**: comparar só a word alta do resultado antes de qualquer verificação completa.
4. **Loop crítico em IRAM** (`IRAM_ATTR`), ambos os cores alimentando o pipeline, mínimo de ticks do RTOS para Wi-Fi/display.
5. Meta de pesquisa (M3/M4): **1 MH/s** — paridade com o NMMiner via assembly/unrolling, sem reaproveitar código dele (modelo de licença/ativação incompatível; apenas SparkMiner e TRM como fontes).

- Benchmark integrado: comando serial + `/api/bench` + card no dashboard (kH/s por backend: SW / HW-baseline / HW-pipeline).
- Realismo: números de marketing (~1000 kH/s) são medidos sem display ativo; no S3 o motor SHA tem perfil diferente do clássico — validar cada chip separadamente.

---

## 5. Estrutura do fork

```
criptohost-nerdos/                   (fork de BitMaker-hub/NerdMiner_v2)
├── platformio.ini
├── LICENSE                          # GPL-3.0 (herdada)
├── README.md · CONTRIBUTING.md · CHANGELOG.md · BRANDING.md
├── src/
│   ├── main.cpp
│   ├── mining/   miner · sha256_hw (NOVO) · sha256_sw · stratum
│   ├── net/      wifi_mgr · mdns_svc (NOVO)
│   ├── web/      server (NOVO) · ws_push (NOVO) · ota (NOVO)
│   ├── state/    monitor · settings (NVS)
│   ├── market/   prices (NOVO)
│   └── drivers/  display/ · thermal
├── data/                            # LittleFS → dashboard tema Cripto Host
│   ├── index.html · fleet.html · config.html · ota.html
│   ├── app.js · ch-theme.css        # paleta #0B041A + identidade CH
├── boards/ · docs/ · tools/flasher/ · tools/bench/ · test/
└── docs/SOLO-NODE.md                # guia nó próprio (digibyte-ckpool Docker)
```

### 5.1 platformio.ini (targets iniciais)

```ini
[platformio]
default_envs = ch-devkit-v1, ch-esp32s3

[env]
framework = arduino
monitor_speed = 115200
lib_deps =
    https://github.com/tzapu/WiFiManager
    esphome/ESPAsyncWebServer-esphome
    bblanchon/ArduinoJson
board_build.filesystem = littlefs
build_flags =
    -D CH_VERSION='"v0.1.0-alpha"'
    -D CORE_DEBUG_LEVEL=0

[env:ch-devkit-v1]              ; Tier 1 — "Recomendado"
platform = espressif32
board = esp32dev
build_flags = ${env.build_flags} -D BOARD_DEVKIT_V1 -D NO_DISPLAY -D USE_HW_SHA

[env:ch-esp32s3]                ; Tier 1 — headless S3
platform = espressif32
board = esp32-s3-devkitc-1
build_flags = ${env.build_flags} -D BOARD_S3_HEADLESS -D NO_DISPLAY -D USE_HW_SHA

[env:ch-tdisplay-s3]            ; Tier 1 — com display
platform = espressif32
board = lilygo-t-display-s3
build_flags = ${env.build_flags} -D BOARD_TDISPLAY_S3 -D USE_HW_SHA
lib_deps = ${env.lib_deps}
    bodmer/TFT_eSPI

[env:ch-esp32c3]                ; Tier 2 — Fase 3 (porte RISC-V)
platform = espressif32
board = esp32-c3-devkitm-1
build_flags = ${env.build_flags} -D BOARD_C3 -D NO_DISPLAY -D USE_HW_SHA -D SINGLE_CORE
```

---

## 6. Detalhamento das fases em Milestones e Issues (GitHub-ready)

> Convenções: labels `type:` (feat, chore, docs, test, port), `area:` (mining, web, fleet, ota, config, brand, ci, idf), `tier:` (1, 2, 3). Estimativas em dias ideais de 1 dev. Cada issue nasce com critérios de aceite (CA) copiáveis para o corpo.

### 🏁 Milestone M0 — Fundação (v0.1.0-alpha) · 2–3 semanas

| # | Issue | Labels | Est. | Critérios de aceite |
|---|---|---|---|---|
| M0-01 | Fork do NerdMiner_v2 + renomeação CriptoHost NerdOS (sem tocar no core) | chore | 1d | Repo `criptohost-nerdos` público; README com atribuição ao upstream e ao escopo de referência; LICENSE GPL-3.0 mantida |
| M0-02 | CI GitHub Actions com build matrix dos 3 targets Tier 1 | chore, ci | 2d | Push → 3 `.bin` como artifacts; badge no README; falha de build bloqueia merge |
| M0-03 | Targets `ch-devkit-v1`, `ch-esp32s3`, `ch-tdisplay-s3` no platformio.ini | chore, ci | 1d | `pio run -e <target>` compila os 3 sem warnings críticos |
| M0-04 | Migração SPIFFS → LittleFS | chore, area:web | 1d | Config sobrevive a reflash de firmware (partição preservada) |
| M0-05 | Refatorar estado global para `state/monitor` (single source of truth) | chore, area:mining | 3d | Todas as métricas (shares, hashrate, temp, uptime) lidas de uma struct única thread-safe |
| M0-06 | Validação de mineração nos 3 targets em `eu.digi.hmpool.io:3337` | test | 2d | 24 h contínuas por target; shares aceitos registrados; zero reboots |
| M0-07 | BRANDING.md + kit visual Cripto Host (paleta, logo, tipografia) | docs, area:brand | 2d | Paleta com base #0B041A documentada; assets em `/data/brand/`; zero assets de terceiros (O7) |
| M0-08 | Release automatizada (tag → GitHub Release com `.bin` + changelog) | chore, ci | 1d | `git tag v*` publica release com os 3 binários nomeados |

**Critério de saída M0:** upstream intacto minerando nos 3 targets, CI verde, identidade definida.

### ⚡ Milestone M1 — Performance + Dashboard (v0.5.0-beta) · 4–6 semanas

| # | Issue | Labels | Est. | Critérios de aceite |
|---|---|---|---|---|
| M1-01 | Módulo `sha256_hw`: acesso ao periférico SHA (DevKit V1/D0WD) | feat, area:mining | 5d | ≥350 kH/s sustentado; resultados idênticos ao SW em suite de vetores de teste |
| M1-02 | Módulo `sha256_hw`: variante ESP32-S3 | feat, area:mining | 3d | ≥300 kH/s sustentado; mesmos vetores de teste |
| M1-03 | Midstate caching no pipeline de mineração | feat, area:mining | 3d | Ganho medido documentado; shares válidos na pool |
| M1-04 | Fallback automático HW→SW + flag `USE_HW_SHA` | feat, area:mining | 1d | S2/targets sem HW compilam e mineram via SW sem mudança de código |
| M1-05 | Benchmark integrado (serial + endpoint `/api/bench`) | feat, test | 2d | Comando retorna kH/s por método (HW/SW) em JSON |
| M1-06 | ESPAsyncWebServer + rota `/api/status` (contrato §4.2) | feat, area:web | 3d | JSON completo validado; latência <100 ms; sem impacto >2% no hashrate |
| M1-07 | WebSocket `/ws` com push de telemetria a cada 5 s | feat, area:web | 2d | 2+ clientes simultâneos sem memory leak em 24 h |
| M1-08 | Dashboard Home (tema Cripto Host): hashrate, gauge, shares, temp, RSSI, uptime | feat, area:web, area:brand | 5d | Paridade funcional com a referência de escopo; responsivo mobile; tema #0B041A |
| M1-09 | Card de preços BTC/DGB/XEC (CoinGecko, cache 5 min) | feat, area:web | 2d | Falha de fetch não afeta mineração; indicador "market updated Xm ago" |
| M1-10 | Share data stream + connection log no dashboard | feat, area:web | 2d | Últimos N eventos visíveis; rejeições destacadas |
| M1-11 | mDNS `_criptohost._tcp` com TXT (worker, fw, hardware) | feat, area:fleet | 1d | Nó descoberto via `dns-sd`/avahi em <5 s |
| M1-12 | Teste de estabilidade 7 dias (2 placas) | test | 7d | O5 atingido: uptime ≥7d, eficiência ≥95% |
| M1-13 | **Pipeline SHA de registros diretos** (ref. SparkMiner + TRM Espressif) — backend `HwPipeline` atrás de `IShaBackend` | feat, area:mining, stretch | 8d | Stretch: ≥700 kH/s no DevKit V1; shares válidos na pool (zero rejeição por hash inválido); seleção de backend por flag `SHA_BACKEND=pipeline`; fallback para baseline se falhar auto-teste de vetores no boot |

**Critério de saída M1:** O1 e O2 atingidos; dashboard completo com identidade CH. **O1b (M1-13) é stretch — não bloqueia a saída do M1**, mas seu resultado define se o marketing da v1.0 usa "350+" ou "700+".

### 🛰 Milestone M2 — Fleet + Config + OTA (v1.0.0) · 4–5 semanas

| # | Issue | Labels | Est. | Critérios de aceite |
|---|---|---|---|---|
| M2-01 | Página Fleet: descoberta mDNS + cards de dispositivos | feat, area:fleet | 4d | ≥95% dos nós descobertos em ≤30 s (O3); card com hashrate/temp/Wi-Fi/pool/versão/status |
| M2-02 | Agregados da frota (online, hashrate total, temp média) | feat, area:fleet | 1d | Valores batem com soma dos `/api/status` individuais |
| M2-03 | Ações remotas: Restart, abrir Config/OTA do peer | feat, area:fleet | 2d | Restart confirma com dialog |
| M2-04 | Página Configuration (pool/port/wallet.worker/senha) + Save & Restart | feat, area:config | 3d | Persistência em NVS; validação de campos; reinício aplica config |
| M2-05 | Perfis de pool prontos (DGB/BTC/XEC/BCH) selecionáveis | feat, area:config | 2d | Dropdown preenche endpoint/porta; docs/POOLS.md com status de cada pool |
| M2-06 | OTA via upload `.bin` (Update.h) com barra de progresso | feat, area:ota | 4d | O4: sucesso ≥99% em 50 updates de teste; config preservada; aviso "não desconecte" |
| M2-07 | Proteção OTA: validação de magic byte/tamanho + retry | feat, area:ota | 2d | Upload de arquivo inválido rejeitado sem brick |
| M2-08 | Factory Reset com confirmação dupla | feat, area:config | 1d | Factory reset limpa NVS+LittleFS config e volta ao captive portal |
| M2-09 | Captive portal re-skin `CriptoHostNerdOS` (provisionamento) | feat, area:brand | 2d | Fluxo primeiro-boot completo com identidade CH |
| M2-10 | Web flasher em `nerdos.cripto.host` (GitHub Pages/ESP Web Tools) | feat, area:ci | 3d | Flash via Chrome/Edge funcional para os 3 targets |
| M2-11 | Docs completas: ARQUITETURA, API, HARDWARE, POOLS, SOLO-NODE (PT-BR/EN) | docs | 4d | Guia de nó próprio digibyte-ckpool testado end-to-end |
| M2-12 | Frota piloto: 5+ nós mistos (DevKit+S3) por 7 dias | test | 7d | O3/O4/O5 verificados em ambiente real; issues de campo triadas |

**Critério de saída M2:** **v1.0.0** — paridade funcional total com o escopo de referência, identidade 100% Cripto Host, release pública.

### 🔀 Milestone M3 — Porte C3/C6 + Camada de portabilidade (v1.2.0) · 4–6 semanas

| # | Issue | Labels | Est. | Critérios de aceite |
|---|---|---|---|---|
| M3-01 | Abstração `IMinerBackend` (dual-core vs single-core) | feat, area:mining, port | 3d | Tier 1 sem regressão de hashrate após refactor |
| M3-02 | Porte do loop de mineração p/ RISC-V single-core (C3) | feat, port, tier:2 | 5d | ≥250 kH/s estável no C3 com HW SHA |
| M3-03 | Target `ch-esp32c6` | feat, port, tier:2 | 2d | C6 minerando com paridade de features do C3 |
| M3-04 | Adapters de portabilidade: `IStorage`, `IHttpServer`, `IOta`, `IMdns` | chore, area:idf | 5d | Core sem `#include <Arduino.h>` fora de `src/adapters/` |
| M3-05 | Task/heap tuning para single-core (web + mining simultâneos) | feat, area:mining | 3d | Dashboard responsivo no C3 sem derrubar hashrate >5% |
| M3-06 | CI expandida (5 targets) + testes de regressão de hashrate | chore, ci | 2d | Benchmark automático comparando build atual vs anterior |
| M3-07 | **Pesquisa 1 MH/s no ESP32 clássico** (assembly do hot loop, unrolling, tuning de barramento) | feat, area:mining, research | 10d | Relatório com kH/s por técnica; ≥900 kH/s = sucesso; resultados aplicados ao backend `HwPipeline` sem código de terceiros não-GPL |

**Critério de saída M3:** Tier 2 entregue; código pronto para o strangler pattern da Fase 4.

### 🚀 Milestone M4 — Migração ESP-IDF 6.0 + Next-gen (v2.0.0) · 6–10 semanas

| # | Issue | Labels | Est. | Critérios de aceite |
|---|---|---|---|---|
| M4-01 | Build "Arduino as IDF component" (pioarduino/IDF 5.5+) | chore, area:idf | 4d | Mesmo código compila via toolchain IDF; `sdkconfig` versionado |
| M4-02 | Adapter NVS nativo (`nvs_flash`) substitui Preferences | feat, area:idf | 2d | Config migra automaticamente; frota mista OK |
| M4-03 | Adapter `esp_http_server` + WS nativo substitui AsyncWebServer | feat, area:idf, area:web | 5d | Mesmo contrato `/api/*`; carga de 5 clientes estável |
| M4-04 | OTA A/B com `esp_ota_ops` + rollback automático | feat, area:idf, area:ota | 4d | Update com falha simulada → rollback e boot na partição anterior |
| M4-05 | Provisionamento nativo substitui WifiManager (SoftAP + DNS) | feat, area:idf | 4d | Fluxo primeiro-boot idêntico; Arduino removido do link final |
| M4-06 | Bump ESP-IDF 6.0 (migração guiada + Picolibc) | chore, area:idf | 5d | 5 targets compilam no IDF 6.0; changelog de breaking changes |
| M4-07 | Targets C5 e C61 + bootloader OTA seguro (recovery) | feat, tier:3 | 4d | C5/C61 minerando; teste de recovery do bootloader documentado |
| M4-08 | Target experimental P4+C6 (esp-hosted SDIO) | feat, tier:3 | 8d | P4 minerando via Wi-Fi do C6; benchmark publicado (candidato a recorde) |
| M4-09 | Stratum V2 opt-in (Noise_NX + libsecp256k1, ref. PRs Bitaxe/NerdQAxePlus) | feat, area:mining | 8d | Handshake SV2 com pool compatível; fallback V1 automático |
| M4-10 | Release v2.0.0 + comunicação de migração para a comunidade | docs, chore | 2d | Guia de upgrade; frotas mistas v1/v2 interoperando via `/api/*` |

**Regra de ouro (invariante em M4):** o contrato `/api/*` e o formato de config em NVS **não mudam** — frotas mistas Arduino/IDF convivem durante toda a transição.

---

## 7. Matriz de hardware

| Tier | Placa | Milestone | Meta kH/s | Papel |
|---|---|---|---|---|
| 1 | ESP32 DevKit V1 (D0WD) | M0–M2 | ≥350 · stretch ≥700 · pesquisa 1 MH/s | "Recomendado" — custo mínimo, **maior teto comprovado** (mesmo silício dos miners comerciais de ~1000 kH/s) |
| 1 | ESP32-S3 headless | M0–M2 | ≥300 · stretch a validar | Nó de frota moderno (motor SHA com perfil diferente do clássico) |
| 1 | LilyGO T-Display S3 | M1–M2 | ≥300 | Variante com display (display ativo reduz o teto) |
| 2 | ESP32-C3 | M3 | ≥250 | Nó econômico/baixo consumo |
| 2 | ESP32-C6 / C61 | M3–M4 | ≥250 | Sucessor do C3, Wi-Fi 6 |
| 3 | ESP32-C5 | M4 | ≥250 | Dual-band + bootloader OTA seguro |
| 3 | ESP32-P4 + C6 | M4 | recorde? | Flagship experimental (2× RISC-V @400 MHz) |

---

## 8. Licenciamento, marca e riscos

**Licença de código:** GPL-3.0 obrigatória (derivado do NerdMiner_v2). Referências permitidas: SparkMiner, BitMiner24 (GPL), NerdOS/AxeOS (GPL). **Proibido:** reaproveitar código do NMMiner (modelo de ativação incompatível).

**Marca vs. código (importante):** GPL cobre o código, não a marca. A identidade Cripto Host (logo, nome, paleta, mascote se houver) fica protegida via `BRANDING.md` com cláusula de uso de marca — terceiros podem forkar o código, mas devem remover a marca Cripto Host (mesmo modelo usado por projetos como o AxeOS). Da mesma forma, nosso projeto não usa nenhum asset do Ha•Kou NerdOS — apenas a referência funcional de escopo.

**Atribuição:** crédito explícito no README ao NerdMiner_v2 (BitMaker e time) como upstream, e menção ao ecossistema (HAN, SparkMiner, BitMiner24) nas partes inspiradas.

**Riscos:**

| Risco | Prob. | Mitigação |
|---|---|---|
| HW SHA abaixo da meta no S3 | Média | Priorizar D0WD; benchmark contínuo (M1-05); piso aceitável 250 kH/s |
| Pipeline de registros (M1-13) gerar hashes inválidos (race CPU↔periférico) | Média | Auto-teste de vetores no boot com fallback p/ baseline; validar em pool com zero rejeição por hash inválido antes de promover o backend |
| Contaminação de licença ao estudar o NMMiner | Baixa | Regra clean-room: apenas SparkMiner (GPL) e TRM da Espressif como fontes; documentar origem de cada técnica no PR |
| Heap insuficiente (web + TLS preços + mining) | Média | Preços via cache/proxy; WS payload compacto; PSRAM no S3 |
| Pools low-diff saírem do ar | Baixa | Perfis múltiplos (M2-05) + guia nó próprio (M2-11) |
| Breaking changes IDF 6.0 | Alta (esperado) | Strangler pattern (M4-01→06); adapters isolam impacto |
| AsyncWebServer instável sob carga | Média | Antecipação do M4-03 se necessário |
| Confusão de marca com Ha•Kou | Baixa | O7 + BRANDING.md; identidade e naming 100% próprios desde M0-07 |

---

## 9. Definição de pronto (v1.0.0)

1. Os 3 targets Tier 1 compilam no CI e mineram DGB/BTC/XEC/BCH estáveis por 7 dias.
2. Dashboard, Fleet, Configuration e OTA com paridade funcional total ao escopo de referência — com identidade 100% Cripto Host.
3. O1–O7 verificados e documentados no repo.
4. Web flasher público em `nerdos.cripto.host` + guias PT-BR/EN.
5. Milestones M3/M4 publicados como issues no GitHub.

---

## 10. Handoff para a branch de testes (Claude Code)

> Instruções para a sessão do projeto validar as novas possibilidades de hashrate em uma branch isolada, sem afetar o caminho da v1.0.

**Branch:** `research/sha-pipeline` (a partir de `main` pós-M0). Nada desta branch entra em `main` sem passar pelos critérios de M1-13.

**Objetivo da validação:** medir, no mesmo hardware (ESP32 DevKit V1 / WROOM-32), o kH/s real de cada backend SHA e confirmar se o stretch de ≥700 kH/s é atingível com código GPL-limpo.

**Passos sugeridos:**
1. Criar a interface `IShaBackend` (`src/mining/sha_backend.h`) com três implementações: `SwBackend` (nerdSHA256plus atual), `HwBaselineBackend` (mbedTLS/`esp_sha`) e `HwPipelineBackend` (registros diretos + pipelining).
2. Implementar `HwPipelineBackend` usando como referência **apenas** o SparkMiner (GPL) e o ESP32 Technical Reference Manual (cap. SHA Accelerator) — registrar a origem de cada técnica em comentários.
3. Auto-teste no boot: 8 vetores conhecidos (header→hash esperado); backend só é ativado se passar; caso contrário fallback silencioso para baseline com log.
4. `tools/bench/bench.py`: lê a serial por 60 s e reporta média/mínimo/máximo de kH/s por backend; rodar 3× cada, com e sem Wi-Fi conectado.
5. Teste de campo: 2 h em `eu.digi.hmpool.io:3337` por backend; critério = **zero shares rejeitados por "invalid hash"** (rejeições por stale/duplicate são aceitáveis).
6. Registrar tudo em `docs/BENCH-SHA.md` (tabela: backend × chip × Wi-Fi on/off × display on/off) e abrir o PR referenciando M1-13.

**Sinais de decisão:**
- ≥700 kH/s com shares 100% válidos → promover `HwPipelineBackend` como default do DevKit V1 e atualizar marketing para "700+ kH/s".
- 450–700 kH/s → manter como opt-in (`SHA_BACKEND=pipeline`) e seguir com M3-07.
- <450 kH/s ou hashes inválidos intermitentes → arquivar a branch com o relatório e manter baseline; reavaliar no IDF 6.0 (M4).

**Não fazer nesta branch:** tocar em web/fleet/OTA, alterar o contrato `/api/*`, ou copiar qualquer trecho de firmwares com licença de ativação (NMMiner).

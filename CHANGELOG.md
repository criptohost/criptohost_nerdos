# Changelog — CriptoHost NerdOS

Formato: [Keep a Changelog](https://keepachangelog.com/pt-BR/) · versionamento [SemVer](https://semver.org/lang/pt-BR/).

## [Unreleased]

## [v0.3.0-alpha] — 2026-09-04

Pipeline SHA de registros diretos (M1-13, §4.3 nível 2): DevKit V1 sai de ~377 para **~705 kH/s sustentado**. Relatório completo em [docs/BENCH-SHA.md](docs/BENCH-SHA.md).

### Added
- Camada `IShaBackend` (`src/mining/sha_backend.*`) com três backends: `sw` (nerdSHA256plus), `hw-baseline` (mbedTLS) e `hw-pipeline`; seleção por `-D SHA_BACKEND="pipeline|baseline|sw"` (ligado no env `ch-devkit-v1`).
- `hw-pipeline` no ESP32 clássico: laço interno em asm Xtensa (ref. SparkMiner v3, GPL-3.0, + TRM Espressif) com bloco 2 escrito enquanto o motor processa o bloco 1, early-reject de 16 bits e IRAM. No S3/C3, midstate injetado em `SHA_H_n`. Variante C atrás de `-D SHA_PIPE_C`.
- Auto-teste no boot em dois estágios: 8 vetores conhecidos por backend + cross-check de lote do pipeline contra mbedTLS; reprovação cai para baseline → sw com log.
- Conferência em software de cada candidato HW antes do submit; divergência vira evento `test` e conta em `hw_mismatch` no `/api/bench`.
- `/api/bench` com kH/s por backend medidos no boot; linha serial `[BENCH] khs=… hw=… sw=…` 1×/s; `tools/bench/bench.py` (média/mín/máx por N corridas); `test/host/run.sh` (SwBackend nativo contra os vetores).
- Perfil BCH2 na FusionPool (`bch2.fusionpool.pro`): `:4442` para ESP32/Android, `:4443` para CPU ≥100 MH/s; dashboard reconhece o host como BCH2 e linka o painel certo. Tabela de tiers por aparelho em `docs/POOLS.md`.
- Planejamento v2.1 em `docs/PLANEJAMENTO-ESCOPO.md` (§4.3 em dois níveis, O1b, M1-13, §10 handoff).
- Dedicated error log (`GET /api/errors`, WS `errors`): up to 48 rejects/stratum failures, kept separately so they are not evicted by the 24-event live log.
- Per-board identity: setup AP `CriptoHostNerdOS-XXXX` and DHCP/mDNS hostname include a MAC suffix so a second unit can join the same LAN without turning the first one off.
- MAC address on Home worktable and Fleet cards (`mac` in `/api/status`).

### Removed
- Identify (Home/Fleet button, `POST /api/identify`, LED blink). It fought GPIO 2 with the mining LED/SHA path on DevKit.

### Fixed
- Stale-job submits: apply a new `mining.notify` immediately (drop queued shares for the old template, submit before the 50 ms idle wait) so “21 Job not found” is less common.
- OTA prepare: erase the OTA partition on a side task (Arduino `loop` stays free) and wait up to 45 s in the UI. A second Prepare while busy is ignored so a retry does not restart the erase.
- Dual-slot 4 MB partition table (`partitions/ch_4mb_ota.csv`): web OTA was impossible on `huge_app.csv` (only `ota_0`). Needs one USB factory flash; later updates use the other slot.
- Ignore duplicate `mining.notify` (same job_id+ntime); pad submit nonce to 8 hex; hold a share back if the pool already has a new message waiting (avoids sending a job the pool just replaced).

### Changed
- Pool default DGB: `dgb.fusionpool.pro:3332` (FusionPool, BR, sem cadastro) no lugar de `digi.hmpool.io:3337`; é onde a v0.3.0 foi validada. Para os irmãos CPU/Android a recomendação é `:3333`. Dropdown do Config, placeholders, READMEs, POOLS.md, API.md e mock atualizados.
- Workers de mineração unificados em `minerWorker(IShaBackend&)`: HW fixado no core 1 (longe de Wi-Fi/Stratum), SW no core 0; lote de 2048 nonces por lock do motor. Removidas ~770 linhas de código HW SHA específico por alvo em `mining.cpp`.
- CPU fixada em 240 MHz no boot (sdkconfig do Arduino default é 160).
- Pool default DGB: `digi.hmpool.io:3337` (US) instead of `digi.hmpool.io`.
- OTA: pause miner/stratum/monitor and disable Wi-Fi sleep **before** the file is sent; flash writes run on a side task so the upload TCP is not blocked.
- Config page: Pool and Wi-Fi tabs. `POST /api/wifi` changes the home network without the captive portal.
- Setup AP SSID and captive title: `CriptoHostNerdOS` (was `CriptoHostAP`). Password unchanged (`MineYourCoins`).

## [v0.1.0-alpha] — 2026-08-15

Fork inicial do [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) com a camada CriptoHost (M0 + código de M1/M2).

### Added
- Targets PlatformIO `ch-devkit-v1`, `ch-esp32s3`, `ch-tdisplay-s3` (Tier 1) e `ch-esp32c3` (Tier 2, experimental) com `CH_BUILD`.
- Dashboard web local (LittleFS, tema Cripto Host `#0B041A`): Home, Fleet, Config, OTA.
- API fleet: `GET /api/status` (contrato §4.2), `/api/events`, `/api/config` GET/POST, `/api/restart`, `/api/factory-reset`, `/api/fleet`, `/api/bench`, WebSocket `/ws` com push de 5 s.
- Descoberta mDNS `_criptohost._tcp` com TXT (worker, fw, hardware) — fleet peer-to-peer sem servidor central.
- OTA via upload de `.bin` no navegador com barra de progresso, validação de magic byte e config preservada.
- Contadores de shares sent/accepted/rejected no core (contrato fleet).
- Card de preços BTC/DGB/XEC (CoinGecko via browser, cache 5 min).
- Perfis de pool DGB/BTC/XEC/BCH na página Config.
- CI GitHub Actions: build matrix dos 3 targets Tier 1 + release automatizada por tag.
- BRANDING.md (marca protegida, GPL 7e), kit visual em `data/brand/`, flag `CH_UNBRANDED` para forks.

### Changed
- Provisionamento: AP `CriptoHostAP`; pool default DGB `digi.hmpool.io:3337` (builds CH).
- Persistência de config em LittleFS (antes SPIFFS) nos builds CH.

### Herdado do upstream (intacto)
- Mining core SHA-256d com acelerador de hardware + midstate caching (nerdSHA256plus), Stratum V1 completo, WiFiManager, drivers de display.

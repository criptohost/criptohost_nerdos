# Changelog — CriptoHost NerdOS

Formato: [Keep a Changelog](https://keepachangelog.com/pt-BR/) · versionamento [SemVer](https://semver.org/lang/pt-BR/).

## [Unreleased]

### Added
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

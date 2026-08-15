# Changelog — CriptoHost NerdOS

Formato: [Keep a Changelog](https://keepachangelog.com/pt-BR/) · versionamento [SemVer](https://semver.org/lang/pt-BR/).

## [Unreleased]

## [v0.1.0-alpha] — 2026-08-15

Fork inicial do [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) com a camada CriptoHost (M0 + código de M1/M2).

### Added
- Targets PlatformIO `ch-devkit-v1`, `ch-esp32s3`, `ch-tdisplay-s3` (Tier 1) e `ch-esp32c3` (Tier 2, experimental) com `CH_BUILD`.
- Dashboard web local (LittleFS, tema Cripto Host `#0B041A`): Home, Fleet, Config, OTA.
- API fleet: `GET /api/status` (contrato §4.2), `/api/events`, `/api/config` GET/POST, `/api/identify`, `/api/restart`, `/api/factory-reset`, `/api/fleet`, `/api/bench`, WebSocket `/ws` com push de 5 s.
- Descoberta mDNS `_criptohost._tcp` com TXT (worker, fw, hardware) — fleet peer-to-peer sem servidor central.
- OTA via upload de `.bin` no navegador com barra de progresso, validação de magic byte e config preservada.
- Contadores de shares sent/accepted/rejected no core (contrato fleet).
- Card de preços BTC/DGB/XEC (CoinGecko via browser, cache 5 min).
- Perfis de pool DGB/BTC/XEC/BCH na página Config.
- CI GitHub Actions: build matrix dos 3 targets Tier 1 + release automatizada por tag.
- BRANDING.md (marca protegida, GPL 7e), kit visual em `data/brand/`, flag `CH_UNBRANDED` para forks.

### Changed
- Provisionamento: AP `CriptoHostAP`; pool default DGB `eu.digi.hmpool.io:3337` (builds CH).
- Persistência de config em LittleFS (antes SPIFFS) nos builds CH.

### Herdado do upstream (intacto)
- Mining core SHA-256d com acelerador de hardware + midstate caching (nerdSHA256plus), Stratum V1 completo, WiFiManager, drivers de display.

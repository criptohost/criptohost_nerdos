<div align="center">

<img src="data/brand/icon-192.png" width="90" alt="CriptoHost">

# CriptoHost NerdOS

**Transforme um ESP32 de R$ 30 em um minerador SHA-256 de verdade — com dashboard web, frota e OTA.**

🇧🇷 Português · [🇺🇸 English](README.en.md)

[![build](https://github.com/criptohost/criptohost_nerdos/actions/workflows/build.yml/badge.svg)](https://github.com/criptohost/criptohost_nerdos/actions)
[![release](https://img.shields.io/github/v/release/criptohost/criptohost_nerdos?label=release)](https://github.com/criptohost/criptohost_nerdos/releases)
[![license](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)

*Um produto [Cripto Host](https://cripto.host) — "Miner de um jeito fácil"*

</div>

---

## 🧭 O que é isso?

O CriptoHost NerdOS é um **firmware open-source de mineração** para placas ESP32. Ele conecta a placa numa pool real, minera SHA-256d de verdade e entrega tudo num **dashboard web local** — sem app, sem nuvem, sem cadastro.

> ⚠️ **Enquadramento honesto**: isto é um projeto de **hobby e educação**. Um ESP32 DevKit V1 faz ~700 kH/s; um ASIC faz 200 TH/s — 300 milhões de vezes mais. Você não vai enriquecer: vai **aprender** como Stratum, dificuldade, shares e blocos funcionam, vendo tudo acontecer ao vivo. O retorno esperado é ~zero, e dizemos isso com orgulho.

## ✨ O que ele faz

- ⛏️ **Minera de verdade** — SHA-256d no acelerador de hardware do ESP32 com pipeline de registros em asm (~705 kH/s sustentado no DevKit V1, [bench](docs/BENCH-SHA.md)), Stratum V1 completo
- 📊 **Dashboard ao vivo** — hashrate com anel de progresso, shares, eficiência, best difficulty, temperatura, Wi-Fi, preços de mercado, log de conexão
- 🕸️ **Fleet peer-to-peer** — cada nó descobre os vizinhos via mDNS; qualquer placa mostra a frota inteira, sem servidor central
- 🔁 **Lista de peers replicada** — para nós fora do alcance do mDNS (VPS, Android): edite a lista na tela Fleet de **qualquer** nó — placa incluída — e todos os outros sincronizam sozinhos em ~1 min (revisão mais nova vence)
- 🔄 **OTA pelo navegador** — atualize o firmware sem cabo USB, com dois slots (A/B) e configuração preservada
- 📱 **Vira app no celular** — PWA: "Adicionar à Tela de Início" no iPhone/Android
- 🛰️ **Órbita da rede** — mapa vivo da frota no Fleet: seus nós conectados em anéis ao redor do nó atual
- 🔭 **Enxerga mineradores de terceiros** — Bitaxe, NerdQAxe/NerdOctaxe (família AxeOS) e ASICs Antminer (stock ou Braiins OS, via API CGMiner) na mesma rede aparecem em cartão âmbar com hashrate, temperatura, pool e versão (descoberta feita pelos nós CPU/CH Agent)
- 🌐 **Provisionamento fácil** — primeiro boot abre o portal Wi-Fi `CriptoHostAP`
- 🪙 **Multi-moeda SHA-256d** — DigiByte (default, shares frequentes), BTC, BCH, XEC, PPC, BC2, BCH2 — e Namecoin + Fractal Bitcoin de graça via merged mining na BCMonster

## 🖼️ Telas

Capturas reais de um ESP32 DevKit V1 minerando (frota de validação: placas + Mac + servidor + Android no mesmo Fleet).

| | Desktop | Mobile |
|---|---|---|
| **Home** | ![Home](docs/screenshots/esp32-home-desktop.png) | <img src="docs/screenshots/esp32-home-mobile.png" width="260"> |
| **Fleet** | ![Fleet](docs/screenshots/esp32-fleet-desktop.png) | <img src="docs/screenshots/esp32-fleet-mobile.png" width="260"> |
| **Config** | ![Config](docs/screenshots/esp32-config-desktop.png) | <img src="docs/screenshots/esp32-config-mobile.png" width="260"> |
| **OTA** | ![OTA](docs/screenshots/esp32-ota-desktop.png) | <img src="docs/screenshots/esp32-ota-mobile.png" width="260"> |

## 🚀 Comece em 10 minutos

**Você vai precisar de:** uma placa ESP32 (DevKit V1 ~R$ 30, ESP32-S3 ou LilyGO T-Display S3), cabo USB e uma carteira da moeda que quer minerar (DigiByte recomendado).

1. **Baixe o firmware** da sua placa na [página de Releases](https://github.com/criptohost/criptohost_nerdos/releases) — pegue o arquivo `*-full.bin` (já vem com bootloader, app e dashboard).
2. **Grave na placa** no offset `0x0` (esptool, ESP Flash Tool ou o web flasher):
   ```bash
   pip install esptool
   esptool.py write_flash 0x0 criptohost-nerdos-vX.Y.Z-ch-devkit-v1-full.bin
   ```
3. **Conecte no Wi-Fi da placa** — rede `CriptoHostAP` (senha `MineYourCoins`) — e siga o portal: escolha sua rede, informe `carteira.worker`.
4. **Abra o dashboard** — `http://ch-XXXX.local` (o nome aparece no portal) ou o IP da placa. Em ~30 segundos os primeiros shares aceitos aparecem no log. 🎉
5. **(Opcional)** No celular: Safari/Chrome → Compartilhar → **Adicionar à Tela de Início** — vira um app.

> 💡 Atualizações futuras: pela própria página **OTA** do dashboard, com o arquivo `*-ota.bin` — sem cabo.

## 🕸️ A frota (e os irmãos do ecossistema)

Todo nó CriptoHost fala o mesmo contrato: anuncia `_criptohost._tcp` via mDNS e responde `GET /api/status`. Resultado: **placas, PCs, servidores e celulares aparecem no mesmo Fleet**, cada um com seu card, agregados e ações remotas (Home, Config, Restart).

| Repositório | O que minera | Hashrate típico | Pool DGB recomendada |
|---|---|---|---|
| **criptohost_nerdos** (este) | ESP32 (DevKit V1, S3, T-Display) | ~705 kH/s (DevKit V1) · ~300 kH/s (S3) | `dgb.fusionpool.pro:3332` (default) |
| [criptohost_cpuminer](https://github.com/criptohost/criptohost_cpuminer) | Windows, Linux e macOS (CPU) | 20–165 MH/s | `dgb.fusionpool.pro:3333` |
| [criptohost_mobile](https://github.com/criptohost/criptohost_mobile) | Android via Termux (iOS = painel) | ~47 MH/s | `dgb.fusionpool.pro:3333` |

## 🔌 API

Cada nó expõe uma API HTTP local (a mesma que o dashboard usa):

```bash
curl http://ch-XXXX.local/api/status
```

```json
{
  "worker": "CH-8634", "hardware": "ESP32 DevKit V1", "status": "mining",
  "hashrate_khs": 381.0, "temp_c": 47.0, "rssi_dbm": -35,
  "shares": {"found": 251, "sent": 251, "accepted": 241, "rejected": 10, "pending": 0},
  "best_difficulty": 7.16, "valid_blocks": 0
}
```

Rotas: `/api/status` · `/api/config` · `/api/fleet` · `/api/peers` · `/api/events` · `/api/identify` · `/api/restart` · `/api/ota`. Documentação completa em [docs/API.md](docs/API.md). **Este contrato é congelado** — é ele que faz gerações e plataformas diferentes conviverem na mesma frota.

## ⛏️ Pools e moedas

O default é **DigiByte na FusionPool** (`dgb.fusionpool.pro:3332`, password `X`; BR, sem cadastro): dificuldade baixa = shares aceitos com frequência = feedback constante para aprender. Para os irmãos CPU/Android a recomendação é a porta `3333` (tier micro miners) da mesma pool. Perfis prontos no Config para BTC (lottery), BCH, XEC e PPC — detalhes e status de cada pool em [docs/POOLS.md](docs/POOLS.md). Quer independência total? Monte seu próprio nó+pool: [docs/SOLO-NODE.md](docs/SOLO-NODE.md).

## ❓ Perguntas honestas

**Vou ganhar dinheiro?** Não. Em DGB você acumula frações de centavo; em BTC a chance de bloco é ~1 em milhões de anos. O produto é o aprendizado (e o card de shares aceitos no seu dashboard).

**Quanto gasta de energia?** ~1–2 W por placa — menos que um LED. Pode deixar ligado sem culpa.

**Esquenta?** 45–60 °C minerando, normal para o chip. O dashboard mostra a temperatura ao vivo.

**Preciso de conta em algum lugar?** Só uma carteira da moeda (a pool default não exige cadastro). Nada de nuvem ou telemetria nossa.

## 🧑‍💻 Para desenvolvedores

```bash
git clone https://github.com/criptohost/criptohost_nerdos && cd criptohost_nerdos
pip install platformio
pio run -e ch-devkit-v1              # compila (targets: ch-devkit-v1, ch-esp32s3, ch-tdisplay-s3)
pio run -e ch-devkit-v1 -t upload    # grava app via USB
pio run -e ch-devkit-v1 -t uploadfs  # grava dashboard (LittleFS)
python3 tools/mock/mock_server.py    # dashboard no navegador sem placa (http://localhost:8091)
```

Arquitetura, mapa de código e decisões: [docs/ARQUITETURA.md](docs/ARQUITETURA.md) · Roadmap e milestones: [docs/PLANEJAMENTO-ESCOPO.md](docs/PLANEJAMENTO-ESCOPO.md) · Como contribuir: [CONTRIBUTING.md](CONTRIBUTING.md).

## 📜 Licença, marca e créditos

- **Código**: [GPL-3.0](LICENSE), herdada do upstream [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) (BitMaker e comunidade) — todo o crédito do core de mineração a eles.
- **Marca**: código livre, marca protegida — nome, logo e identidade Cripto Host seguem o [BRANDING.md](BRANDING.md). Forke à vontade; remova a marca (`-D CH_UNBRANDED` ajuda).
- Inspirações da categoria: HAN, SparkMiner, BitMiner24.

## 💬 Contato

Dúvidas, ideias ou parceria: **fale@cripto.host** · Issues e PRs são bem-vindos.

---

<div align="center">

*Small power. Big learning.* 💜

</div>

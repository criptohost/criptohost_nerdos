# CriptoHost NerdOS

**Firmware open-source de mineração SHA-256 para ESP32** — telemetria ao vivo, controle de frota, configuração de pool e atualizações OTA, tudo em um só sistema.

Um produto [Cripto Host](https://cripto.host) · *"Node de um jeito fácil"*

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]() [![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)]() [![Firmware](https://img.shields.io/badge/firmware-v0.1.0--alpha-8b5cf6)]()

---

## ⚠️ Aviso honesto

Este é um projeto de **hobby, educação e comunidade** (lottery mining). Um ESP32 a ~300 kH/s tem probabilidade estatisticamente desprezível de encontrar um bloco. O valor está em aprender Stratum, SHA-256d, dificuldade e sistemas embarcados — não em retorno financeiro.

## Funcionalidades

- ⛏ **Mining core** SHA-256d com acelerador de hardware + midstate caching (Stratum V1)
- 📊 **Dashboard web** local: hashrate, shares, eficiência, temperatura, Wi-Fi, preços de mercado
- 🛰 **Fleet**: descoberta automática (mDNS) e gestão de todos os nós da sua rede
- ⚙️ **Configuração via web**: pool, worker e wallet sem recompilar
- 📦 **OTA**: atualize o firmware pelo navegador, sem cabo USB
- 🪙 **Multi-moeda SHA-256d**: DigiByte (default), Bitcoin, eCash e Bitcoin Cash

## Hardware suportado

| Tier | Placa | Hashrate alvo |
|---|---|---|
| 1 | ESP32 DevKit V1 (recomendado) | ≥350 kH/s |
| 1 | ESP32-S3 (headless ou T-Display S3) | ≥300 kH/s |
| 2 | ESP32-C3 / C6 *(em desenvolvimento)* | ≥250 kH/s |
| 3 | ESP32-C5 / P4+C6 *(experimental)* | — |

## Início rápido

1. **Flash**: acesse o web flasher em `nerdos.cripto.host` (Chrome/Edge) ou baixe o `.bin` nas [Releases]().
2. **Provisionamento**: conecte-se ao AP `CriptoHostNerdOS-XXXX` (único por placa) e configure o Wi-Fi.
3. **Acesse o dashboard**: `http://<ip-do-dispositivo>` (descubra pelo roteador ou mDNS).

Documentação completa em [`/docs`](docs/): [Arquitetura](docs/ARQUITETURA.md) · [API](docs/API.md) · [Pools](docs/POOLS.md) · [Hardware](docs/HARDWARE.md) · [Nó próprio](docs/SOLO-NODE.md)

## Compilando do código

```bash
git clone https://github.com/criptohost/criptohost-nerdos
cd criptohost-nerdos
pio run -e ch-devkit-v1        # ou ch-esp32s3 / ch-tdisplay-s3
pio run -e ch-devkit-v1 -t upload
```

---

## 📜 Licença e marca — leia antes de forkar

Este projeto separa explicitamente **código** e **marca**:

### Código: GPL-3.0 (livre)

Todo o código-fonte deste repositório é licenciado sob **[GPL-3.0](LICENSE)**, como trabalho derivado do [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2). Você pode usar, estudar, modificar e redistribuir o código, inclusive comercialmente, desde que mantenha a mesma licença e o código aberto.

### Marca: Cripto Host (protegida — todos os direitos reservados)

O nome **"Cripto Host"**, o nome do produto **"CriptoHost NerdOS"**, o logotipo, a identidade visual, o design system e demais assets de marca (arquivos em `/data/brand/`) **não são cobertos pela GPL** e permanecem propriedade exclusiva da Cripto Host. A GPL-3.0 (seção 7e) permite expressamente essa reserva de direitos de marca em trabalhos derivados.

**Se você forkar este projeto, deve:**
1. **Remover** o nome, logo e assets visuais da Cripto Host do seu fork;
2. **Renomear** o produto (não use "CriptoHost", "Cripto Host" ou variações confundíveis);
3. **Manter** a licença GPL-3.0, os avisos de copyright e a atribuição ao código original.

Detalhes completos em [`BRANDING.md`](BRANDING.md). Uso comercial da marca somente com autorização por escrito: **contato@cripto.host**.

### Atribuições

- **Upstream**: [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) (BitMaker e comunidade) e [HAN](https://github.com/valerio-vaccaro/HAN) (valerio-vaccaro) — GPL-3.0.
- **Referências técnicas** (aceleração SHA por hardware, arquitetura de fleet): SparkMiner, BitMiner24 Firmware, NerdOS/AxeOS — todos GPL.
- Este projeto **não** utiliza código, assets ou marca de nenhum outro produto de mineração; semelhanças funcionais refletem o escopo comum da categoria "home mining ESP32".

---

*Feito com 💜 pela comunidade Cripto Host — Small power. Big learning.*

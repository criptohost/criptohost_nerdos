# Pools — CriptoHost NerdOS

> EN summary: profiles for SHA-256d lottery/PPLNS mining. DigiByte on hmpool is the default (low diff, frequent shares = better learning feedback). Availability of community pools changes — check before relying on one.

Perfis disponíveis no dropdown da página **Config**:

| Moeda | Pool | Endpoint | Modelo | Status* |
|---|---|---|---|---|
| DGB (default) | hmpool | `eu.digi.hmpool.io:3337` | PPLNS low-diff | ✅ ativo |
| BTC | nerdminers.org | `pool.nerdminers.org:3333` | lottery | ✅ ativo |
| BTC | nerdminer.io | `pool.nerdminer.io:3333` | lottery | ✅ ativo |
| BTC | public-pool.io | `public-pool.io:21496` | lottery/solo | ✅ ativo |
| XEC | mining-dutch | `xec.mining-dutch.nl:9998` | PPLNS (conta própria) | ⚠️ requer registro |
| BCH | mining-dutch | `bch.mining-dutch.nl:9985` | PPLNS (conta própria) | ⚠️ requer registro |

\* verificado em 2026-08. Pools comunitárias low-diff mudam — se um perfil morrer, abra issue (`area:config`).

## Formato de credenciais

- **user**: `carteira.worker` — ex.: `DAbC...xyz.CH-DevKit-01`. O sufixo após o ponto vira o nome do nó no fleet.
- **password**: `x` na maioria; `d=0.001` para sugerir dificuldade baixa em pools que aceitam (`suggest_difficulty`).

## Expectativa honesta

Um ESP32 (~350 kH/s) em lottery mining de BTC tem chance ~zero de bloco. Para feedback frequente de shares aceitos, use o perfil default DGB low-diff. O objetivo é aprender Stratum/SHA-256d/dificuldade — não renda.

## Nó próprio

Quer independência de pools de terceiros? Guia completo em [SOLO-NODE.md](SOLO-NODE.md) (digibyte-ckpool em Docker).

# Pools — CriptoHost NerdOS

> EN summary: profiles for SHA-256d lottery/PPLNS mining. DigiByte on FusionPool is the default (`:3332` for ESP32, `:3333` recommended for CPU/Android; low diff, frequent shares = better learning feedback). Availability of community pools changes — check before relying on one.

An ESP32 speaks Stratum and SHA-256d. Any pool that accepts that protocol can be pointed at from **Config**. What it cannot do is compete with ASICs: BTC/BCH network difficulty makes a block from ~350 kH/s a lottery ticket. Prefer low-diff DGB if you want accepted shares on screen.

Profiles in the Config dropdown (verified 2026-08):

| Coin | Pool | Endpoint | Model | Notes |
|---|---|---|---|---|
| DGB | hmpool | `digi.hmpool.io:3337` | PPLNS low-diff | US region; password `X` maiúsculo obrigatório |
| DGB/BTC/BCH/PPC/BC2/BCH2 | BCMonster | `bcmonster.com` portas lottery: DGB `9996` · BTC `9999` (+NMC/FB merged grátis) · BCH `9994` · PPC `9995` · BC2 `9997` · BCH2 `9998` | PROP 1.5% (ou solo com senha `m=solo`) | Sem cadastro; diff 0.001 alvo 1 share/10s; dashboard `bcmonster.com/worker.html?address=WALLET`; estreia BC2/BCH2/NMC/FB no ecossistema. Validada ao vivo (share aceito em 1.1s) |
| **DGB (default)** | FusionPool | `dgb.fusionpool.pro:3332` (ESP32) · `:3333` (CPU/Android) | Híbrida 70/25/5 (taxa 2%) | BR; sem cadastro; porta 3332 = default do NerdOS (validada: 705 kH/s, 97,7 % de shares aceitos, ver BENCH-SHA); 3333 = micro miners (20 kH/s–99 GH/s, vardiff 0.001+), recomendada para os irmãos CPU/Android; 3334 Bitaxe · 3335/3336 ASICs; achador leva 70%, 25% dividido por shares×peso (10%/dia até 100%), 5% jackpot |
| DGB | letsmine.it | `us1.letsmine.it:3335` | solo, no signup | Brazil/US region; vardiff — try password `d=0.001` |
| BTC | nerdminers.org | `pool.nerdminers.org:3333` | lottery | NerdMiner-class |
| BTC | nerdminer.io | `pool.nerdminer.io:3333` | lottery | NerdMiner-class |
| BTC | public-pool.io | `public-pool.io:21496` | lottery/solo | |
| BTC | pyblock | `pool.pyblock.xyz:3333` | lottery | |
| BTC | sethforprivacy | `pool.sethforprivacy.com:3333` | lottery | |
| BTC | solomining.de | `pool.solomining.de:3333` | lottery | |
| BTC | letsmine.it | `us1.letsmine.it:3332` | solo | Chance ≈ 0 at ESP32 hashrate |
| BCH | letsmine.it | `us1.letsmine.it:3334` | solo (PROP `:3434`) | No signup |
| XEC | letsmine.it | `us1.letsmine.it:3333` | solo (PROP `:3433`) | No signup |
| PPC | letsmine.it | `us1.letsmine.it:3347` | solo | Peercoin |
| XEC | mining-dutch | `xec.mining-dutch.nl:9998` | PPLNS | Requires an account |
| BCH | mining-dutch | `bch.mining-dutch.nl:9985` | PPLNS | Requires an account |

LetsMine also lists other SHA-256 coins (BC2, MYT, BTCS, XRO, BCH2). They are valid Stratum targets if you type host+port by hand; they are omitted from the dropdown and from Home prices because CoinGecko coverage is weak.

Other LetsMine regions: `gb1` `pl1` `de1` `fr1` `sg1` — same ports, swap the hostname.

## Finding the device after first Wi-Fi setup

The captive portal only asks for the home network. After Save, the setup AP closes. Open `http://<hostname>.local` on the home Wi-Fi (same name in the router DHCP list). Each board gets a unique chip suffix (last 2 MAC bytes), so a second unit does not collide with `ch-01`. Factory default worker is `CH-<chip>` (e.g. `CH-A3F2` → `http://ch-a3f2.local`). The setup AP is `CriptoHostNerdOS-XXXX`. You can still rename `wallet.worker` in Config; the `.local` name stays unique.

## Formato de credenciais

- **user**: `carteira.worker` — ex.: `DAbC...xyz.CH-A3F2`. O sufixo após o ponto vira o nome do nó no fleet; o hostname mDNS acrescenta o chip se o worker for compartilhado.
- **password**: `x` na maioria; `d=0.001` para sugerir dificuldade baixa em pools com vardiff (`suggest_difficulty`).

## Expectativa honesta

Um ESP32 (~350 kH/s) em lottery mining de BTC/BCH tem chance ~zero de bloco. Para feedback frequente de shares aceitos, use o perfil default DGB low-diff. O objetivo é aprender Stratum/SHA-256d/dificuldade — não renda.

## Nó próprio

Quer independência de pools de terceiros? Guia completo em [SOLO-NODE.md](SOLO-NODE.md) (digibyte-ckpool em Docker).


## Avaliadas e descartadas (com o motivo medido)

Para ninguém perder tempo redescobrindo — probe Stratum feito em 24/08/2026:

| Pool | Motivo |
|---|---|
| SoloFury (BTC/BCH/BC2/BCH2/XEC) | piso de vardiff **100.000** — ESP32 levaria ~34 anos por share; só ASIC |
| Kryptex solo (BCH `bch.kryptex.network:7015`) | aceita wallet sem conta, mas piso **524.288** — só ASIC |
| unMineable (sha256) | autoriza `COIN:wallet`, mas diff **fixa em 16.384** (2 min sem vardiff cair) — inviável até para CPU; confirma o relato da comunidade de que subiram o piso |
| zpool (sha256) | piso **65.536** — só ASIC |
| solopool.org (dgb-sha) | stratum sem resposta + relato "hashrate 0" na comunidade NerdMiner |
| molepool (XEC) | stratum sem resposta |
| blockminerz · bchsolo · vkbit · zergpool · zSolo | DNS não resolve — extintas (ainda circulam em listas antigas) |

# Pools — CriptoHost NerdOS

> EN summary: profiles for SHA-256d lottery/PPLNS mining. DigiByte on hmpool is the default (low diff, frequent shares = better learning feedback). Availability of community pools changes — check before relying on one.

An ESP32 speaks Stratum and SHA-256d. Any pool that accepts that protocol can be pointed at from **Config**. What it cannot do is compete with ASICs: BTC/BCH network difficulty makes a block from ~350 kH/s a lottery ticket. Prefer low-diff DGB if you want accepted shares on screen.

Profiles in the Config dropdown (verified 2026-08):

| Coin | Pool | Endpoint | Model | Notes |
|---|---|---|---|---|
| DGB (default) | hmpool | `digi.hmpool.io:3337` | PPLNS low-diff | US region; best feedback on ESP32 |
| DGB | FusionPool | `dgb.fusionpool.pro:3333` | Híbrida 70/25/5 (taxa 2%) | BR; sem cadastro; porta 3333 = micro miners (20 kH/s–99 GH/s, vardiff 0.001+); 3334 Bitaxe · 3335/3336 ASICs; achador leva 70%, 25% dividido por shares×peso (10%/dia até 100%), 5% jackpot |
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

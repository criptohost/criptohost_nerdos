# Salvium (SAL) na FusionPool — o que muda e o que falta

> Avaliação de 2026-09-07, antes do lançamento. Validado por handshake real contra `sal.fusionpool.pro:3443`.

## O que é

- **SAL = Salvium**, moeda da família Monero (CryptoNote). Algoritmo **RandomX** (`rx/0` no XMRig), **só CPU**.
  Não é SHA-256d: nada do que a placa e o CPUMiner fazem hoje serve para SAL.
- **Pool:** `sal.fusionpool.pro:3443`, já no ar. A página chama de "stratum seguro", mas a porta é **TCP puro**
  (TLS falha com EOF); o protocolo é o JSON-RPC estilo Monero (`login`/`job`/`submit`), o que o XMRig fala nativo.
  Login com endereço inválido devolve `{"error":{"code":-1,"message":"Invalid address"}}`.
- **Carteira:** a pool pede "carteira Salvium (Carrot)". Prefixos mainnet (fonte: `cryptonote_config.h`):
  `SaLv…` (legado), `SaLvi…`/`SaLvs…` (integrado/sub), **`SC1…`** (Carrot), `SC1i…`/`SC1s…`. 8 casas decimais.
- **User:** `CARTEIRA.WORKER`, password `x`. Mesmo modelo 70/25/5 com taxa 2 %.

## Requisitos do RandomX (o que limita cada projeto)

| Requisito | Valor | Consequência |
|---|---|---|
| CPU 64 bits com JIT | x86-64, ARM64 (RISC-V64) | 32 bits só pelo interpretador, "muito mais lento": TV box 32 bits fora |
| RAM modo rápido | 2080 MiB compartilhados | celular/TV box com < 2,5 GB livres cai no modo leve |
| RAM modo leve | 256 MiB | ~5× mais lento (i9-9900K: 5770 → 1160 H/s) |
| Huge pages | opcional, +10–30 % | sem root no Android; no PC exige config |
| Minerador | **XMRig** (GPL-3, compatível com a nossa licença) | não há RandomX no cpuminer-opt nem no cpuminer-multi |
| Build XMRig no Termux | `cmake`, `libuv`, `openssl`; `-DWITH_HWLOC=OFF -DARM_TARGET=8` | só aarch64 |

Ordem de grandeza: PC desktop 2–8 kH/s, celular aarch64 0,3–1,5 kH/s, ESP32 **zero**.

## Nossos projetos hoje

| Projeto | Minera SAL? | Estado | O que falta |
|---|---|---|---|
| **NerdOS (ESP32)** | **Não, nunca** (520 KB de RAM vs 256 MiB mínimos) | Dashboard já mostra nós CPU de qualquer moeda no Fleet | Só exibição: reconhecer host `sal.` como SAL no `miningSymbol()`, preço SAL no ticker (CoinGecko id `salvium`), **não** listar SAL no dropdown da placa |
| **CPUMiner (Win/Linux/macOS)** | Não | Motor único (cpuminer-opt, SHA-256d); agent lê API `KEY=VAL` na 4048 | Segundo motor **XMRig** escolhido pelo `algo` do perfil; agent lendo a API HTTP do XMRig (`GET /2/summary`: `hashrate.total[0]`, `results.shares_good`, `shares_total`, `results.best`); perfil `sal-fusionpool.json` (config XMRig, não cpuminer); validação de carteira `SaLv…`/`SC1…`; scripts de build com cmake |
| **Mobile (Android/Termux)** | Não | Build só do cpuminer | Build do XMRig no `build-android.sh` (aarch64), modo leve automático quando faltar RAM, aviso de bateria/temperatura; TV box 32 bits fica de fora |

O contrato do Fleet (`/api/status`) não precisa mudar: `hashrate_khs` aceita 0,8 kH/s de RandomX tão bem quanto 705 kH/s de SHA.
Sugestão aditiva: campo opcional `"algo": "sha256d|randomx"` para o card mostrar a unidade certa e o dashboard não
comparar H/s de RandomX com kH/s de SHA como se fossem a mesma coisa.

## Plano sugerido (ordem de valor)

1. **CPUMiner: motor XMRig + agent com duas APIs + perfil SAL** — é onde SAL vira real (2–3 dias).
   XMRig como submódulo/clone em `third_party/xmrig`, binário em `./xmrig`; `mine.sh` e `agent.py` escolhem o
   binário pelo `algo` do perfil; agent normaliza as duas APIs para o mesmo `summary` interno.
2. **Dashboards (placa + CPU): símbolo, preço e validação de carteira** — meio dia, replicado pelo `sync-web.sh`.
   Inclui a validação que faltou no BCH2 (host `bch2.` → carteira `bitcoincashii:`; host `sal.` → `SaLv`/`SC1`).
3. **Mobile: XMRig no Termux + docs** — 1 dia. Celulares aarch64 com ≥3 GB rendem em modo rápido.
4. **Teste de campo** na `sal.fusionpool.pro:3443` com carteira Carrot real, 2 h, shares aceitos, antes de anunciar.

## Riscos e decisões pendentes

- **Doação do XMRig:** o padrão é 1 % (mínimo configurável 0 % só recompilando). Decidir a política e documentar.
- **"Seguro" sem TLS:** login e shares vão em texto puro. Não é problema de segurança para mineração, mas o
  README não deve repetir "seguro" sem explicar.
- **Calor e bateria no Android:** RandomX carrega 100 % da CPU por muito mais tempo que SHA-256d por share;
  manter o aviso "na tomada" e o limite de threads como default.
- **Limite por IP na FusionPool DGB** (visto em 2026-09-07, portas 3332/3333 recusando conexões novas): perguntar
  à pool antes de somar mais um endpoint por rede.

## Além do SAL: Monero e outras moedas de CPU

O trabalho do SAL (motor escolhido por `algo` do perfil, agent lendo mais de uma API, símbolo/preço/validação
de carteira no dashboard) é **genérico**. Feito uma vez, abre três famílias:

| Família | Motor | Moedas (exemplos) | Custo extra depois do SAL |
|---|---|---|---|
| **RandomX** (`rx/*`) | XMRig | **Monero (XMR, `rx/0`)**, Salvium (SAL), Wownero (WOW, `rx/wow`), ArQmA (`rx/arq`) | Só perfis + símbolo/preço |
| **GhostRider / Argon2** | XMRig | Raptoreum (RTM, `gr`), Chukwa | Só perfis |
| **yespower / yescrypt / minotaurx** | **cpuminer-opt, que já temos** | Yenten (`yespowerr16`), CPUchain (`yespower`), BitZeny (`yescryptr8`), Koto (`yescryptr8g`), MicroBitcoin (`power2b`), Litecoin Cash (`minotaurx`) | Zero motor novo: só a seleção de `algo` por perfil (hoje `-a sha256d` está fixo no `mine.sh` e no agent) |

**Sobre "baixa dificuldade":** a dificuldade da share é da pool (vardiff), não da moeda. O que muda entre moedas
é a dificuldade da *rede* (chance de bloco) e quão amigável o algoritmo é a CPU/celular. Para o objetivo do projeto
(shares aceitos aparecendo com frequência), qualquer pool com vardiff serve; Monero e as yespower são as que dão
retorno visível numa CPU comum porque ASICs não competem.

Candidatas por aparelho:

| Aparelho | Melhor aposta | Por quê |
|---|---|---|
| PC (x86-64, ≥4 GB) | **Monero** via XMRig, modo rápido | maior liquidez, pools com porta "low-end" (SupportXMR 3333, MoneroOcean 10001) e vardiff desde 10 k |
| Celular aarch64 ≥3 GB | Monero/SAL em modo rápido (0,3–1,5 kH/s) ou **VerusCoin** (VerusHash 2.2, 1–4 MH/s em celular — mas exige outro motor, `ccminer-verus`/hellminer) | RandomX cabe; VerusHash é o algoritmo desenhado para celular |
| TV box 32 bits | **yescrypt** via cpuminer-multi (`-a yescrypt`, BitZeny/Koto) | RandomX não roda em 32 bits; yescrypt roda |
| ESP32 | continua só SHA-256d | 520 KB de RAM |

**Ordem que recomendo:** (1) `algo` por perfil em `mine.sh`/`agent.py` + 3–4 perfis yespower/yescrypt: barato,
sem motor novo, e já dá "outras moedas"; (2) XMRig com SAL **e Monero** no mesmo passo, porque a diferença é um
perfil; (3) VerusCoin só se a meta for celular — é um terceiro motor.

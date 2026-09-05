# Bench SHA-256d por backend

> Escopo §4.3 (níveis 1 e 2) · handoff §10 · issues M1-01 / M1-02 / M1-05 / M1-13 · branch `research/sha-pipeline` → v0.3.0-alpha

## Backends (§10 passo 1)

Interface `IShaBackend` em `src/mining/sha_backend.h`: `prepare(header)` uma vez por job, `scan(nonce, count, hash)`
varre nonces e devolve o hash completo quando passa o early-reject de 16 bits. Os dois workers de mineração
(`minerWorker` em `src/mining.cpp`) só conhecem a interface.

| Backend | Implementação | Origem | Compressões/nonce |
|---|---|---|---|
| `sw` | nerdSHA256plus (midstate + bake) | NerdMiner_v2, GPL-3.0 | 2 em software |
| `hw-baseline` | `mbedtls_sha256_ret` ×2 (periférico via `CONFIG_MBEDTLS_HARDWARE_SHA`) | ESP-IDF | 3 via driver mbedTLS |
| `hw-pipeline` (D0WD) | laço interno em asm Xtensa: registradores diretos, bloco 2 escrito enquanto o motor calcula o bloco 1, early-reject, IRAM (`-D SHA_PIPE_C` = variante C, ~20 % mais lenta) | SparkMiner `sha256_pipelined_v3.cpp` (GPL-3.0) + ESP32 TRM cap. SHA Accelerator | 3 no periférico (sem midstate: o clássico não tem `SHA_H_n` gravável) |
| `hw-pipeline` (S3/C3) | midstate injetado em `SHA_H_n` + 2 blocos | minerWorkerHw S3 do NerdMiner_v2 | 2 no periférico |

Cada técnica do pipeline está marcada no código com `[SM]` (SparkMiner) ou `[TRM]`, como pede o §10 passo 2.
Nada do NMMiner ou de firmware com licença de ativação.

## Auto-teste no boot (§10 passo 3)

Dois estágios. **(1) 8 vetores conhecidos** por backend, acerto em k=0. **(2) Cross-check de lote** no `hw-pipeline`:
1,5 s de varredura com header aleatório, cada acerto de 16 bits conferido contra mbedtls com o nonce devolvido;
qualquer divergência reprova o backend e cai para baseline com log. O estágio 2 existe por causa do incidente abaixo.

### Incidente 2026-09-04: asm v7 passou nos 8 vetores e gerou 61/64 shares inválidos

Sintoma na pool: `23 low difficulty share of ~5e-10` (hash aleatório para o nonce enviado) enquanto o firmware via
diff 0,005; só os ~3 % de shares do worker `sw` eram aceitos. Causa: no `asm volatile`, `k` (contador, começa em 0)
era `"+r"` e `zero` era `"r"(0)`; sem early-clobber o GCC deu o **mesmo registrador** aos dois, e o `addi k` corrompia
os "zeros" do bloco 2 a partir da 2ª iteração. Os vetores passam porque acertam em k=0. Fix: `"+&r"` em `k` e `ns`
(`a6`/`a10` vs zeros em `a8` no disassembly); cross-check 19/19 e hashrate inalterado.


`sha_backend_boot()` roda os 8 vetores de `test/sha_vectors.json` (gerados por `test/test_sha_vectors.py`,
todos com hash terminando em 16 bits zero) em cada backend, mede ~250 ms de kH/s isolado e escolhe o backend
do worker HW: `SHA_BACKEND` (`"baseline"` default; `"pipeline"` opt-in, ligado no env `ch-devkit-v1`; `"sw"`) só é honrado se passar nos vetores;
senão cai para `hw-baseline` → `sw`, com log no serial e no `/api/bench`.

```
[CH] SHA bench: sw 45.2 kH/s ok | hw-baseline 31.0 kH/s ok | hw-pipeline 712.4 kH/s ok -> hw worker=hw-pipeline (SHA_BACKEND=pipeline)
```

(números ilustrativos do formato, não medições)

No host, sem placa: `sh test/host/run.sh` compila o `SwBackend` nativo e valida os 8 vetores
(passa em 2026-09-04).

## Bench de 60 s (§10 passo 4)

```bash
~/.platformio/penv/bin/python tools/bench/bench.py /dev/cu.usbserial-0001 --seconds 60 --runs 3 --label "wifi on"
```

Lê a linha `[BENCH] khs=<total> hw=<backend> sw=<backend>` que o monitor imprime 1×/s e reporta média/mín/máx
por corrida. O total é core 0 (backend HW) + core 1 (`sw`). Para comparar backends no mesmo hardware:

```bash
pio run -e ch-devkit-v1 -t upload                                    # pipeline (default)
PLATFORMIO_BUILD_FLAGS='-D SHA_BACKEND=\"baseline\"' pio run -e ch-devkit-v1 -t upload
PLATFORMIO_BUILD_FLAGS='-D SHA_BACKEND=\"sw\"' pio run -e ch-devkit-v1 -t upload
```

"Sem Wi-Fi": suba o nó sem credenciais salvas (captive portal ativo, sem pool) — o worker HW não recebe jobs
nesse estado, então o número isolado por backend é o da linha de boot; o de 60 s só vale com Wi-Fi + pool.

## Resultados

Medido em 2026-09-04 num **ESP32-D0WD-V3 rev 3.1 (DevKit V1, CP2102)**, target `ch-devkit-v1`, pool
`dgb.fusionpool.pro:3332` (FusionPool, DGB), Wi-Fi conectado, sem display. S3 e T-Display S3 compilam mas não havia placa: pendentes.

### Isolado por backend (linha de boot, motor sem concorrência, 8/8 vetores em todos)

| Chip | Target | `sw` kH/s | `hw-baseline` kH/s | `hw-pipeline` kH/s | Vetores | Meta |
|---|---|---|---|---|---|---|
| ESP32 D0WD (DevKit V1) | `ch-devkit-v1` | 41.1 | 13.6 | **666.7** (asm v7; C v4: 547.1) | 8/8 · 8/8 · 8/8 | baseline ≥350 ✅ · stretch ≥700 ✅ (sustentado 705 com `sw` no core 0) |
| ESP32-S3 (DevKitC) | `ch-esp32s3` | pendente | pendente | pendente | pendente | ≥300 |
| ESP32-S3 (T-Display S3) | `ch-tdisplay-s3` | pendente | pendente | pendente | pendente | ≥300 |

Evolução do `hw-pipeline` no D0WD durante o handoff (isolado / sustentado):

| Versão | Mudança | isolado kH/s | sustentado kH/s |
|---|---|---|---|
| v1 | porte C do SparkMiner v3, leituras `DPORT_REG_READ` | 414.7 | 377 |
| v2 | leitura simples dos regs SHA (`CONFIG_ESP32_DPORT_WORKAROUND=y` torna `DPORT_REG_READ` uma função com interrupções desligadas) | 518.6 | ~440 |
| v3 | worker HW fixado no core 1 (sem Wi-Fi/Stratum) + `memw` só após escrita de controle | 536.1 | ~460 |
| v4 | lote de 2048 nonces por lock do motor (era 256) | 547.1 | ~470 |
| v5 | poll do BUSY via `l32i` em asm (sem `memw` do `-mserialize-volatile`) | 531.3 | ~475 (sem `sw`: ~540) |
| v6 | CPU forçada a 240 MHz (já estava; sdkconfig diz 160) | 531.3 | idem |
| v7 | **laço interno em asm Xtensa** (porte do [SM] v3, lotes de 256 com nonce nativo) | **666.7** | **705** com `sw` · 668 sem `sw` |

### Sustentado 60 s × 3 (`bench.py`), core 0 + core 1, D0WD, Wi-Fi on, sem display

| Backend HW (core 1) | SW (core 0) | corrida | média kH/s | mín | máx |
|---|---|---|---|---|---|
| **hw-pipeline v7 (asm)** | sw | 1 | **704.4** | 666 | 721 |
| hw-pipeline v7 (asm) | sw | 2 | 704.3 | 649 | 721 |
| hw-pipeline v7 (asm) | sw | 3 | 706.5 | 672 | 721 |
| hw-pipeline v7 (asm) | — (`CH_NO_SW_WORKER`) | 1 | 667.0 | 637 | 672 |
| hw-pipeline v7 (asm) | — | 2 | 667.6 | 629 | 672 |
| hw-pipeline v7 (asm) | — | 3 | 668.4 | 622 | 672 |
| hw-pipeline v4 (C) | sw | 1 | 449.1* | 0* | 480 |
| hw-pipeline v4 (C) | sw | 2 | 470.8 | 442 | 480 |
| hw-pipeline v4 (C) | sw | 3 | 469.7 | 442 | 480 |
| hw-baseline | sw | 1 | 50.9* | 0* | 65 |
| hw-baseline | sw | 2 | 52.9 | 37 | 58 |
| hw-baseline | sw | 3 | 53.1 | 37 | 67 |
| sw | sw | 1 | 56.3* | 0* | 66 |
| sw | sw | 2 | 59.3 | 45 | 66 |
| sw | sw | 3 | 59.5 | 45 | 66 |

\* corrida 1 inclui o boot (amostras a 0 antes do primeiro job).

Leituras: em C, o pipeline sustentava ~470 kH/s, ~14 % abaixo do isolado (547); sem o worker `sw` sustentava ~540,
ou seja, o `sw` no core 0 custava ~70 kH/s ao HW e devolvia 41 (saldo negativo). O salto para 667 veio do asm:
o compilador não estava gerando o laço enxuto (a diferença não eram as barreiras, testado em v5). `hw-baseline` via mbedTLS é **mais lento que software** (13.6 vs 41):
o driver do IDF trava/destrava o motor e reinicializa o contexto a cada chamada. Dois cores em `sw` dão 59,
não 82: o nerdSHA256plus já satura a memória. Sem Wi-Fi o worker HW não recebe jobs; o isolado de boot é o número
"sem Wi-Fi" por construção (roda antes das tasks).

### Teste de campo (§10 passo 5) — 2 h em `dgb.fusionpool.pro:3332` por backend

Não executado neste handoff (escopo: passos 1–4). A placa ficou minerando com `hw-pipeline`; contadores em
`/api/status` (`shares_sent/accepted/rejected`) e eventos `reject` no dashboard dão o critério "zero invalid hash".

| Backend | Chip | Duração | Shares enviados | Aceitos | `23 low difficulty` (hash errado) | `21 stale` / `job not found` |
|---|---|---|---|---|---|---|
| hw-pipeline v7 asm (após fix `"+&r"`) | D0WD | 51 min | 128 | 125 (97,7 %) | **1** | 1 / 1 |
| hw-pipeline v7 asm (antes do fix) | D0WD | 26 min | 64 | 2 | 61 | 0 |
| hw-pipeline v4 C | D0WD | 1 h 16 | 176 | 174 (98,9 %) | motivo perdido (ring) | ≤2 |
| hw-baseline | D0WD | pendente | | | | |

O 1 hash errado em 128 mostra que o periférico (ou a leitura DPORT do digest) ainda erra raramente. Resposta na
v0.3.0: **cada candidato HW é conferido em software antes do submit** (padrão SparkMiner; 1 double-SHA por acerto
de 16 bits, ~10/s, custo nulo). Divergência vira evento `test` e conta em `hw_mismatch` no `/api/bench`, então
o passo 5 passa a ser medível sem depender da pool: critério = `hw_mismatch` baixo e `rejected` só stale.

Referências herdadas (histórico git do NerdMiner e docs do SparkMiner, não reproduzidas aqui):

| Fonte | Valor |
|---|---|
| SparkMiner README (`sha256_pipelined_v3`, ESP32 clássico, CYD) | ~715–725 kH/s |
| NerdMiner `228ff88` "esp32s3 HW sha256 speed-up" | 252 kH/s (S3, HW) |
| README deste repo (D0WD, HW + SW nos dois cores) | ~350 kH/s |

## Decisão (§10)

**2026-09-04, 1ª rodada (laço em C, 547 isolado / 470 sustentado):** faixa 450–700 → opt-in. Implementado: default
do código `baseline`, env `ch-devkit-v1` liga `-D SHA_BACKEND="pipeline"`.

**2026-09-04, 2ª rodada (laço em asm, 667 isolado / 705 sustentado com `sw`):** sinal "≥700" do §10 atingido.
**Promovido a default do DevKit V1 na v0.3.0-alpha** (flag `-D SHA_BACKEND="pipeline"` no env `ch-devkit-v1`),
com duas redes de proteção: cross-check de lote no boot (reprova → fallback) e conferência em software de cada
candidato antes do submit (hash errado nunca vira share). Manter o worker `sw` no core 0: com o asm ele soma
(+38 kH/s) em vez de atrapalhar. Passo 5 (2 h) fica como acompanhamento via `hw_mismatch`.

Evidência de campo parcial da 1ª rodada (dashboard, 1 h 16 min em `dgb.fusionpool.pro:3332`, laço C):
176 shares enviados, 174 aceitos, 2 rejeitados (98,9 %); motivo dos 2 saiu do ring buffer antes da leitura.

Próximo passo indicado pelo §10 é "M3-07", que não existe na lista do M3 (termina em M3-06, CI + regressão de
hashrate). Assumido M3-06: o `bench.py` desta branch é o candidato a benchmark automático.

Para passar dos 705: o gap restante para o NMMiner (~1000) é o limite de 3 compressões/nonce do D0WD (sem midstate
gravável) mais o barramento DPORT; o SparkMiner para nos mesmos ~715–725. Só assembly mais agressivo
(desenrolar o poll, `s32i.n`) ou o S3 (2 compressões/nonce) mudam isso.

## Riscos conhecidos do `hw-pipeline` D0WD (a validar no primeiro flash)

- Leituras DPORT: BUSY e early-reject usam leitura simples (`SHA_RD`), como o SparkMiner; o digest completo usa
  leitura sequenciada com interrupções desligadas, como o `minerWorkerHw` original. Se aparecer share inválido
  intermitente no passo 5, recompilar com `-D SHA_PIPE_DPORT_SAFE` (custa ~20 %) e comparar.
- `esp_sha_lock_engine` por lote de 2048 nonces. Mover para o job se aparecer no perfil.
- Escritas nos `SHA_TEXT_n` com stores simples + `memw` (sem `REG_WRITE`), como o SparkMiner. É o que dá o overlap;
  se o motor corromper o bloco 1, trocar por `REG_WRITE` e medir a perda.

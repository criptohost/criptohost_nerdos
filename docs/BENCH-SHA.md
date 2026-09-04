# Bench SHA-256d por backend

> Escopo §4.3 (passos 1–4) · issues M1-01 / M1-02 / M1-05 · branch `research/sha-pipeline`

## O que o bench mede

No boot, antes de subir as tasks de mineração (motor SHA sem concorrência), `ch_sha_selftest()`
em `src/mining.cpp` cronometra ~250 ms de double-SHA por backend e conta quantos vetores
pseudo-aleatórios (1000 headers de 80 bytes) o pipeline HW reproduz byte a byte contra o mbedtls.

| Backend | Caminho | Passo §4.3 |
|---|---|---|
| `sw` | `nerd_sha256d_baked` (nerdSHA256plus): midstate + bake, mesmo laço do `minerWorkerSw` | 2 (midstate) + 3 (fallback) |
| `mbedtls` | `mbedtls_sha256_ret` ×2 sobre 80 bytes (usa o periférico se `CONFIG_MBEDTLS_HARDWARE_SHA`) | 1 (alternativa) |
| `hw` | registradores diretos, mesma sequência do `minerWorkerHw` (`esp32-regs` no D0WD, `s3-regs` no S3/C3) | 1 (periférico) |

Um nonce = um double-SHA. kH/s = nonces / ms. O `hw` é o piso do que um core entrega sozinho;
o `live_khs` do minerador soma HW (core 0) + SW (core 1).

## Como rodar

```bash
pio run -e ch-devkit-v1 -t upload && pio device monitor -b 115200 | grep "SHA bench"
```

Saída esperada no serial (também vai para o log de eventos do dashboard):

```
[CH] SHA bench: sw 45.2 kH/s, mbedtls 31.0 kH/s, hw(esp32-regs) 350.1 kH/s, vetores HW==mbedtls 1000/1000
```

Ou depois do boot, pelo endpoint:

```bash
curl -s http://ch-devkit-01.local/api/bench
```

```json
{"sw_khs":45.2,"mbedtls_khs":31.0,"hw_khs":350.1,"hw_backend":"esp32-regs",
 "hw_vectors_ok":1000,"hw_vectors":1000,"live_khs":392.0,"method":"hw"}
```

Os números acima são ilustrativos do formato, não medições.

## Resultados

Estado em 2026-09-04: **nenhuma placa conectada** ao host durante a execução do handoff
(só `/dev/cu.Bluetooth-Incoming-Port` e `/dev/cu.debug-console`). Os três targets Tier 1
compilam com o bench; as células abaixo ficam pendentes até o primeiro flash.

| Placa | Target | `sw` kH/s | `mbedtls` kH/s | `hw` kH/s | Vetores HW | Meta §2 |
|---|---|---|---|---|---|---|
| ESP32 DevKit V1 (D0WD) | `ch-devkit-v1` | pendente | pendente | pendente | pendente | ≥350 |
| ESP32-S3 DevKitC | `ch-esp32s3` | pendente | pendente | pendente | pendente | ≥300 |
| LilyGO T-Display S3 | `ch-tdisplay-s3` | pendente | pendente | pendente | pendente | ≥300 |

Referências herdadas do upstream NerdMiner (histórico git, não reproduzidas aqui):

| Fonte | Valor |
|---|---|
| `228ff88` "esp32s3 HW sha256 speed-up" | 252 kH/s (S3, HW) |
| `278d9ce` "sha dma test; Increase hashrate" | ~212 kH/s |
| README / docs (D0WD, HW + SW nos dois cores) | ~350 kH/s |

## Critério de aceite (M1-01 / M1-02)

- `hw_vectors_ok == hw_vectors` (1000/1000). Qualquer divergência = bug de pipeline, não de velocidade.
- `hw_khs` ≥ meta da placa sustentado; se abaixo, o piso aceitável do §8 é 250 kH/s.
- `sw_khs` serve de baseline do fallback (S2, portes futuros sem periférico).

## Preencher

Após flashar cada placa, copiar a linha `SHA bench` do serial para a tabela de resultados
e commitar neste arquivo.

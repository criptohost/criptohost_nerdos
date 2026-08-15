# Nó próprio (solo/lottery) — digibyte-ckpool em Docker

> EN summary: run your own DigiByte node + ckpool so your CriptoHost NerdOS fleet mines against YOUR node — "Node de um jeito fácil", agora com miner.

Minerar contra o **seu próprio nó** é o espírito Cripto Host: sem depender de pool de terceiros, você valida a cadeia e monta o template do bloco localmente.

## Requisitos

- Docker + docker-compose; 4 GB RAM; ~60 GB de disco (chain DGB).
- Porta 3333 liberada na LAN.

## 1. docker-compose.yml

```yaml
services:
  digibyted:
    image: digibyte/digibyte:latest
    command: >
      digibyted -server=1 -txindex=1
      -rpcuser=ck -rpcpassword=TROQUE_ESTA_SENHA
      -rpcallowip=172.16.0.0/12 -rpcbind=0.0.0.0
    volumes: [dgb-data:/root/.digibyte]
    restart: unless-stopped

  ckpool:
    image: ghcr.io/eze-kiel/ckpool-solo:latest   # ou build local do ckpool com suporte DGB
    depends_on: [digibyted]
    ports: ["3333:3333"]
    volumes: ["./ckpool.conf:/etc/ckpool.conf:ro"]
    restart: unless-stopped

volumes:
  dgb-data:
```

## 2. ckpool.conf

```json
{
  "btcd": [{ "url": "digibyted:14022", "auth": "ck", "pass": "TROQUE_ESTA_SENHA", "notify": true }],
  "serverurl": ["0.0.0.0:3333"],
  "mindiff": 0.0001,
  "startdiff": 0.001,
  "logdir": "/tmp/ck"
}
```

`mindiff` baixo é essencial: ESP32 a ~350 kH/s precisa de dificuldade mínima para submeter shares com frequência.

## 3. Subir e sincronizar

```bash
docker compose up -d
docker compose logs -f digibyted   # aguarde sync completo da chain (~horas)
```

## 4. Apontar a frota

Na página **Config** de cada nó (ou via `POST /api/config` em lote):

- Pool: `<ip-do-servidor>` · Port: `3333`
- Wallet: seu endereço DGB + `.CH-<modelo>-<nn>`

## 5. Verificar

```bash
docker compose logs -f ckpool     # deve mostrar authorized + shares
curl http://<ip-do-nó>/api/status # accepted crescendo
```

## Solução de problemas

| Sintoma | Causa provável |
|---|---|
| `authorize` falha | endereço DGB inválido para a rede (use endereço legacy/segwit válido) |
| Shares 0 | `mindiff` alto demais; confira `startdiff` |
| Conexão recusa | digibyted ainda sincronizando; ckpool não conecta no RPC |

> Testado end-to-end é critério do M2-11 — reporte divergências via issue `type:docs`.

# Hardware — CriptoHost NerdOS

> EN summary: Tier 1 boards (ESP32 DevKit V1, ESP32-S3, T-Display S3) are supported today; Tier 2 (C3/C6) lands in M3; Tier 3 (C5, P4+C6) in M4.

## Matriz de placas

| Tier | Placa | Target PlatformIO | Meta kH/s | Status | Papel |
|---|---|---|---|---|---|
| 1 | ESP32 DevKit V1 (D0WD) | `ch-devkit-v1` | ≥350 | ✅ suportado | **Recomendado** — custo mínimo, maior teto |
| 1 | ESP32-S3 DevKitC (headless) | `ch-esp32s3` | ≥300 | ✅ suportado | Nó de frota moderno |
| 1 | LilyGO T-Display S3 | `ch-tdisplay-s3` | ≥300 | ✅ suportado | Variante com display |
| 2 | ESP32-C3 | `ch-esp32c3` | ≥250 | 🧪 experimental (M3) | Nó econômico / baixo consumo |
| 2 | ESP32-C6/C61 | — | ≥250 | 📋 planejado (M3–M4) | Sucessor do C3, Wi-Fi 6 |
| 3 | ESP32-C5 | — | ≥250 | 📋 planejado (M4) | Dual-band + OTA seguro |
| 3 | ESP32-P4 + C6 | — | recorde? | 📋 planejado (M4) | Flagship experimental |

## Flash

### Web flasher (recomendado)

`nerdos.cripto.host` (Chrome/Edge, ESP Web Tools) — fonte em `tools/flasher/`.

### Cabo USB

```bash
pio run -e ch-devkit-v1 -t upload      # firmware
pio run -e ch-devkit-v1 -t uploadfs    # dashboard (LittleFS) — necessário no 1º flash
```

## Pinos e LEDs

| Placa | LED de status | Botão |
|---|---|---|
| DevKit V1 | GPIO 2 (onboard) | GPIO 0 (BOOT) |
| S3 DevKitC | RGB GPIO 48 | GPIO 0 |
| T-Display S3 | tela | GPIO 0 / 14 |
| C3 DevKitM | RGB GPIO 8 | GPIO 9 |

## Alimentação e temperatura

- Fonte USB 5 V / ≥500 mA por nó; frotas: hub USB alimentado.
- Temperatura típica minerando: 45–60 °C (sensor interno; precisão ±2 °C — exibida no dashboard e no `/api/status`).
- Uptime alvo ≥7 dias (O5); reinicie via dashboard após atualizações de config.

## Primeiro boot (provisionamento)

1. Conecte no AP **`CriptoHostNerdOS-XXXX`** (XXXX = 4 hex únicos da placa; senha `MineYourCoins`).
2. Portal captive: só a rede Wi-Fi de casa. Pool/wallet já vêm com default.
3. Acesse `http://<hostname>.local` (o portal mostra o nome; no dashboard, Home → Dashboard URL).

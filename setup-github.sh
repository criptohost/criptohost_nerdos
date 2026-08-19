#!/usr/bin/env bash
# ============================================================
# CriptoHost NerdOS — Bootstrap de Milestones e Issues
# Requisitos: gh CLI autenticado (gh auth login) no repo alvo
# Uso: ./setup-github.sh criptohost/criptohost-nerdos
# ============================================================
set -euo pipefail
REPO="${1:?Uso: $0 <owner/repo>}"

echo "==> Criando labels..."
for L in "type:feat|8b5cf6" "type:chore|6b7280" "type:docs|60a5fa" "type:test|34d399" "type:port|c084fc" \
         "area:mining|f59e0b" "area:web|3b82f6" "area:fleet|10b981" "area:ota|ef4444" "area:config|a855f7" \
         "area:brand|ec4899" "area:ci|64748b" "area:idf|f97316" "tier:1|22c55e" "tier:2|eab308" "tier:3|f43f5e"; do
  NAME="${L%%|*}"; COLOR="${L##*|}"
  gh label create "$NAME" --repo "$REPO" --color "$COLOR" --force >/dev/null && echo "  label: $NAME"
done

echo "==> Criando milestones..."
declare -A MS
create_ms() {
  local title="$1" desc="$2"
  local num
  num=$(gh api "repos/$REPO/milestones" -f title="$title" -f description="$desc" --jq .number 2>/dev/null) \
    || num=$(gh api "repos/$REPO/milestones?state=all" --jq ".[] | select(.title==\"$title\") | .number")
  MS["$title"]=$num; echo "  milestone #$num: $title"
}
create_ms "M0 — Fundação (v0.1.0-alpha)"        "Fork, CI, LittleFS, estado global, validação em pool. Saída: upstream minerando nos 3 targets Tier 1."
create_ms "M1 — Performance + Dashboard (v0.5.0-beta)" "HW SHA + midstate (O1), dashboard web ao vivo (O2), mDNS."
create_ms "M2 — Fleet + Config + OTA (v1.0.0)"  "Paridade funcional total do escopo com identidade Cripto Host (O3, O4, O5, O7)."
create_ms "M3 — Porte C3/C6 + Portabilidade (v1.2.0)" "Tier 2 + adapters IStorage/IHttpServer/IOta preparando a migração IDF."
create_ms "M4 — Migração ESP-IDF 6.0 (v2.0.0)"  "Strangler pattern até IDF 6.0 puro; C5/C61, P4+C6 experimental, Stratum V2 opt-in. Invariante: contrato /api/* não muda."

echo "==> Criando issues..."
issue() { # $1 milestone  $2 labels  $3 title  $4 body
  gh issue create --repo "$REPO" --milestone "$1" --title "$3" --body "$4" \
    $(for l in ${2//,/ }; do printf -- '--label %s ' "$l"; done) >/dev/null
  echo "  issue: $3"
}

M0="M0 — Fundação (v0.1.0-alpha)"
issue "$M0" "type:chore" "M0-01 Fork do NerdMiner_v2 + renomeação CriptoHost NerdOS" \
"CA:
- [ ] Repo público com LICENSE GPL-3.0 mantida
- [ ] README com atribuição ao upstream (NerdMiner_v2/HAN) e seção jurídica marca vs código
- [ ] Core de mineração intacto (zero mudanças funcionais)"
issue "$M0" "type:chore,area:ci" "M0-02 CI GitHub Actions com build matrix Tier 1" \
"CA:
- [ ] Push gera 3 .bin como artifacts (devkit-v1, esp32s3, tdisplay-s3)
- [ ] Badge no README; build failure bloqueia merge"
issue "$M0" "type:chore,area:ci" "M0-03 Targets ch-devkit-v1 / ch-esp32s3 / ch-tdisplay-s3" \
"CA:
- [ ] pio run -e <target> compila os 3 sem warnings críticos"
issue "$M0" "type:chore,area:web" "M0-04 Migração SPIFFS → LittleFS" \
"CA:
- [ ] Config sobrevive a reflash (partição preservada)"
issue "$M0" "type:chore,area:mining" "M0-05 Estado global em state/monitor (single source of truth)" \
"CA:
- [ ] Métricas (shares, hashrate, temp, uptime) em struct única thread-safe
- [ ] Telas existentes leem apenas do monitor"
issue "$M0" "type:test" "M0-06 Validação de mineração 24h nos 3 targets (hmpool DGB)" \
"CA:
- [ ] 24h contínuas por target em digi.hmpool.io:3337
- [ ] Shares aceitos registrados; zero reboots"
issue "$M0" "type:docs,area:brand" "M0-07 BRANDING.md + kit visual Cripto Host" \
"CA:
- [ ] Política marca vs código publicada (GPL 7e)
- [ ] Tokens do design system em data/ch-theme.css; assets em /data/brand/
- [ ] O7: zero assets de terceiros"
issue "$M0" "type:chore,area:ci" "M0-08 Release automatizada (tag → Release com .bin)" \
"CA:
- [ ] git tag v* publica release com os 3 binários + changelog"

M1="M1 — Performance + Dashboard (v0.5.0-beta)"
issue "$M1" "type:feat,area:mining" "M1-01 sha256_hw: periférico SHA no DevKit V1 (D0WD)" \
"CA:
- [ ] ≥350 kH/s sustentado
- [ ] Resultados idênticos ao SW na suite de vetores de teste"
issue "$M1" "type:feat,area:mining" "M1-02 sha256_hw: variante ESP32-S3" \
"CA:
- [ ] ≥300 kH/s sustentado; mesmos vetores"
issue "$M1" "type:feat,area:mining" "M1-03 Midstate caching no pipeline" \
"CA:
- [ ] Ganho medido documentado; shares válidos na pool"
issue "$M1" "type:feat,area:mining" "M1-04 Fallback HW→SW + flag USE_HW_SHA" \
"CA:
- [ ] Targets sem HW compilam e mineram via SW sem mudanças"
issue "$M1" "type:feat,type:test" "M1-05 Benchmark integrado (serial + /api/bench)" \
"CA:
- [ ] kH/s por método (HW/SW) em JSON"
issue "$M1" "type:feat,area:web" "M1-06 ESPAsyncWebServer + /api/status (contrato fleet)" \
"CA:
- [ ] JSON completo do contrato §4.2; latência <100ms; impacto ≤2% no hashrate"
issue "$M1" "type:feat,area:web" "M1-07 WebSocket /ws com push 5s" \
"CA:
- [ ] 2+ clientes simultâneos 24h sem memory leak"
issue "$M1" "type:feat,area:web,area:brand" "M1-08 Dashboard Home (tema Cripto Host)" \
"CA:
- [ ] Hashrate/gauge/shares/temp/RSSI/uptime; responsivo mobile
- [ ] 100% tokens ch-theme.css (nenhuma cor hardcoded)"
issue "$M1" "type:feat,area:web" "M1-09 Card de preços BTC/DGB/XEC (cache 5min)" \
"CA:
- [ ] Falha de fetch não afeta mineração; indicador 'updated Xm ago'"
issue "$M1" "type:feat,area:web" "M1-10 Share data stream + connection log" \
"CA:
- [ ] Últimos N eventos; rejeições destacadas"
issue "$M1" "type:feat,area:fleet" "M1-11 mDNS _criptohost._tcp com TXT (worker, fw, hw)" \
"CA:
- [ ] Nó descoberto via dns-sd/avahi em <5s"
issue "$M1" "type:test" "M1-12 Estabilidade 7 dias (2 placas)" \
"CA:
- [ ] O5: uptime ≥7d; eficiência ≥95%"

M2="M2 — Fleet + Config + OTA (v1.0.0)"
issue "$M2" "type:feat,area:fleet" "M2-01 Página Fleet: descoberta + cards" \
"CA:
- [ ] ≥95% dos nós em ≤30s (O3)
- [ ] Card: hashrate/temp/WiFi/pool/versão/status"
issue "$M2" "type:feat,area:fleet" "M2-02 Agregados da frota (online, total kH/s, temp média)" \
"CA:
- [ ] Valores conferem com a soma dos /api/status"
issue "$M2" "type:feat,area:fleet" "M2-03 Ações remotas: Restart, Config/OTA do peer" \
"CA:
- [ ] Restart com confirmação"
issue "$M2" "type:feat,area:config" "M2-04 Página Configuration + Save & Restart (NVS)" \
"CA:
- [ ] Pool/port/wallet.worker/senha persistidos; validação de campos"
issue "$M2" "type:feat,area:config" "M2-05 Perfis de pool DGB/BTC/XEC/BCH" \
"CA:
- [ ] Dropdown preenche endpoint/porta; docs/POOLS.md com status"
issue "$M2" "type:feat,area:ota" "M2-06 OTA via upload .bin com progresso" \
"CA:
- [ ] O4: ≥99% sucesso em 50 updates; config preservada; aviso de energia"
issue "$M2" "type:feat,area:ota" "M2-07 Proteção OTA: validação de binário + retry" \
"CA:
- [ ] Arquivo inválido rejeitado sem brick"
issue "$M2" "type:feat,area:config" "M2-08 Factory Reset (confirmação dupla)" \
"CA:
- [ ] Reset limpa NVS+config e retorna ao captive portal"
issue "$M2" "type:feat,area:brand" "M2-09 Captive portal CriptoHostNerdOS (re-skin)" \
"CA:
- [ ] Fluxo primeiro-boot completo com identidade CH"
issue "$M2" "type:feat,area:ci" "M2-10 Web flasher em nerdos.cripto.host (ESP Web Tools)" \
"CA:
- [ ] Flash via Chrome/Edge para os 3 targets Tier 1"
issue "$M2" "type:docs" "M2-11 Docs: ARQUITETURA/API/HARDWARE/POOLS/SOLO-NODE (PT-BR+EN)" \
"CA:
- [ ] Guia nó próprio digibyte-ckpool testado end-to-end"
issue "$M2" "type:test" "M2-12 Frota piloto 5+ nós mistos por 7 dias" \
"CA:
- [ ] O3/O4/O5 verificados em campo; issues triadas"

M3="M3 — Porte C3/C6 + Portabilidade (v1.2.0)"
issue "$M3" "type:feat,area:mining,type:port" "M3-01 Abstração IMinerBackend (dual vs single-core)" \
"CA:
- [ ] Tier 1 sem regressão de hashrate pós-refactor"
issue "$M3" "type:feat,type:port,tier:2" "M3-02 Porte do miner para RISC-V single-core (C3)" \
"CA:
- [ ] ≥250 kH/s estável no C3 com HW SHA"
issue "$M3" "type:feat,type:port,tier:2" "M3-03 Target ch-esp32c6" \
"CA:
- [ ] C6 com paridade de features do C3"
issue "$M3" "type:chore,area:idf" "M3-04 Adapters IStorage/IHttpServer/IOta/IMdns" \
"CA:
- [ ] Core sem #include <Arduino.h> fora de src/adapters/"
issue "$M3" "type:feat,area:mining" "M3-05 Tuning task/heap single-core (web + mining)" \
"CA:
- [ ] Dashboard responsivo no C3 com perda de hashrate ≤5%"
issue "$M3" "type:chore,area:ci" "M3-06 CI 5 targets + regressão automática de hashrate" \
"CA:
- [ ] Benchmark comparando build atual vs anterior no pipeline"

M4="M4 — Migração ESP-IDF 6.0 (v2.0.0)"
issue "$M4" "type:chore,area:idf" "M4-01 Build 'Arduino as IDF component' (IDF 5.5+)" \
"CA:
- [ ] Mesmo código via toolchain IDF; sdkconfig versionado"
issue "$M4" "type:feat,area:idf" "M4-02 NVS nativo (nvs_flash) substitui Preferences" \
"CA:
- [ ] Migração automática de config; frota mista OK"
issue "$M4" "type:feat,area:idf,area:web" "M4-03 esp_http_server + WS nativo substitui AsyncWebServer" \
"CA:
- [ ] Contrato /api/* inalterado; 5 clientes estáveis"
issue "$M4" "type:feat,area:idf,area:ota" "M4-04 OTA A/B (esp_ota_ops) com rollback automático" \
"CA:
- [ ] Falha simulada → rollback e boot na partição anterior"
issue "$M4" "type:feat,area:idf" "M4-05 Provisionamento nativo substitui WifiManager" \
"CA:
- [ ] Fluxo primeiro-boot idêntico; Arduino fora do link final"
issue "$M4" "type:chore,area:idf" "M4-06 Bump ESP-IDF 6.0 (migração guiada + Picolibc)" \
"CA:
- [ ] 5 targets no IDF 6.0; changelog de breaking changes"
issue "$M4" "type:feat,tier:3" "M4-07 Targets C5/C61 + bootloader OTA seguro (recovery)" \
"CA:
- [ ] C5/C61 minerando; teste de recovery documentado"
issue "$M4" "type:feat,tier:3" "M4-08 Target experimental P4+C6 (esp-hosted SDIO)" \
"CA:
- [ ] P4 minerando via WiFi do C6; benchmark publicado"
issue "$M4" "type:feat,area:mining" "M4-09 Stratum V2 opt-in (Noise_NX + libsecp256k1)" \
"CA:
- [ ] Handshake SV2 com pool compatível; fallback V1 automático"
issue "$M4" "type:docs,type:chore" "M4-10 Release v2.0.0 + guia de migração da comunidade" \
"CA:
- [ ] Guia de upgrade; frotas mistas v1/v2 interoperando via /api/*"

echo ""
echo "✅ Concluído: 16 labels, 5 milestones e 42 issues criadas em $REPO"

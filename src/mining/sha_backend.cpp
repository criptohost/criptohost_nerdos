// CriptoHost NerdOS — §4.3 / §10 passos 1–3: SwBackend, HwBaselineBackend, HwPipelineBackend + selftest/bench.
//
// Origem das técnicas do HwPipelineBackend (ESP32 clássico), por exigência do §10 passo 2:
//  [SM]  SparkMiner, src/mining/sha256_pipelined_v3.cpp, GPL-3.0 (https://github.com/SneezeGUI/SparkMiner)
//  [TRM] ESP32 Technical Reference Manual, cap. "SHA Accelerator" (registradores SHA_TEXT_n,
//        SHA_SHA256_START/CONTINUE/LOAD/BUSY_REG)
// Nenhuma linha vem de firmware com licença de ativação.
#ifndef HOST_TEST
#include <Arduino.h>  // antes do nerdSHA256plus.h: define IRAM_ATTR
#include "mbedtls/sha256.h"
#endif
#include "sha_backend.h"
#include "sha_vectors.h"
#include "../ShaTests/nerdSHA256plus.h"
#include <string.h>
#include <stdio.h>
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef SHA_BACKEND
#define SHA_BACKEND "baseline"  // M1-13: "pipeline" é opt-in por env (decisão 2026-09-04: 470 kH/s sustentado, faixa 450–700 do §10)
#endif

// ---------------- SwBackend: nerdSHA256plus (midstate + bake) — caminho original do NerdMiner ----------------
namespace {
struct SwBackend : IShaBackend {
  uint32_t mid[8], bake[16];
  uint8_t tail[64];  // bytes 64..127 do header (16 de dados + padding)
  const char* name() const override { return "sw"; }
  void prepare(const uint8_t h[128]) override {
    nerd_mids(mid, h);
    nerd_sha256_bake(mid, h + 64, bake);
    memcpy(tail, h + 64, sizeof(tail));
  }
  bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) override {
    for (; count; --count, ++nonce) {
      memcpy(tail + 12, &nonce, 4);
      if (nerd_sha256d_baked(mid, tail, bake, hash)) return true;  // já implementa o early-reject de 16 bits
    }
    return false;
  }
};
SwBackend s_sw;
}
IShaBackend& sha_backend_sw() { return s_sw; }

#ifndef HOST_TEST
// ---------------- HwBaselineBackend: mbedTLS (HW via CONFIG_MBEDTLS_HARDWARE_SHA) ----------------
// Sem midstate: no ESP32 clássico o periférico não aceita estado inicial, e o port mbedTLS do IDF
// converte um contexto clonado para software — então "80 bytes + 32 bytes" é o baseline honesto.
namespace {
struct HwBaselineBackend : IShaBackend {
  uint8_t hdr[80];
  const char* name() const override { return "hw-baseline"; }
  void prepare(const uint8_t h[128]) override { memcpy(hdr, h, 80); }
  bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) override {
    uint8_t inter[32];
    for (; count; --count, ++nonce) {
      memcpy(hdr + 76, &nonce, 4);
      mbedtls_sha256_ret(hdr, 80, inter, 0);
      mbedtls_sha256_ret(inter, 32, hash, 0);
      if (hash[30] == 0 && hash[31] == 0) return true;
    }
    return false;
  }
};
HwBaselineBackend s_baseline;
}
IShaBackend& sha_backend_hw_baseline() { return s_baseline; }

// ---------------- HwPipelineBackend ----------------
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <soc/dport_access.h>
#include <soc/hwcrypto_reg.h>
#include <sha/sha_parallel_engine.h>

#define MEMW()    __asm__ __volatile__("memw" ::: "memory")  // drena o write buffer: após START/CONTINUE/LOAD, antes de ler BUSY [SM]
#define BARRIER() __asm__ __volatile__("" ::: "memory")      // só o compilador: stores ao mesmo bus saem em ordem de programa

namespace {
// Leitura dos registradores SHA (bus DPORT). Default: l32i simples, como o SparkMiner [SM].
// -D SHA_PIPE_DPORT_SAFE usa DPORT_REG_READ (workaround de leitura DPORT do IDF) — mais lento; ligar se
// aparecerem shares inválidos intermitentes (o selftest de 8 vetores não pega corrida entre cores).
#ifdef SHA_PIPE_DPORT_SAFE
#define SHA_RD(addr) DPORT_REG_READ(addr)
#else
// l32i via asm: uma leitura `volatile` em C sai cercada de memw (-mserialize-volatile, default do Xtensa GCC);
// o SparkMiner faz "l32i; bnez" seco no poll [SM]. O "memory" clobber mantém a ordem com os stores.
static inline uint32_t sha_rd_raw(uint32_t addr) { uint32_t v; __asm__ __volatile__("l32i %0, %1, 0" : "=r"(v) : "r"(addr) : "memory"); return v; }
#define SHA_RD(addr) sha_rd_raw(addr)
#endif
static inline void wait_idle() { while (SHA_RD(SHA_256_BUSY_REG)) {} }  // [TRM] SHA_SHA256_BUSY_REG

struct HwPipelineEsp32 : IShaBackend {
  uint32_t w[20];  // header em palavras big-endian: o motor lê SHA_TEXT_n como words BE [TRM]
  const char* name() const override { return "hw-pipeline"; }
  void prepare(const uint8_t h[128]) override {
    for (int i = 0; i < 20; ++i) w[i] = __builtin_bswap32(((const uint32_t*)h)[i]);
  }
  // Loop crítico em IRAM (§4.3 nível 2, técnica 4). Sequência e overlap conforme [SM] v3:
  //  bloco 1 → START → (enquanto o motor calcula) escreve bloco 2 → CONTINUE → LOAD →
  //  padding do 2º SHA por cima do digest → START → LOAD → early-reject de 16 bits.
  // Sem midstate: o ESP32 clássico não tem registradores de estado graváveis (LOAD só copia
  // estado → TEXT) [SM nota v4, TRM]. 3 compressões por nonce é o limite do silício.
  // Laço interno em asm Xtensa — porte de [SM] sha256_pipelined_v3.cpp (GPL-3.0) com duas adaptações:
  //  * lote limitado (`count` ≤ 256) em vez de rodar até achar share; devolve o índice k do acerto ou count;
  //  * nonce nativo: dentro de um sub-bloco de 256 alinhado, bswap(n0+k) == bswap(n0) + (k<<24), então o
  //    contador byte-swapped avança 1<<24 por iteração e o chamador reconstrói o nonce nativo (n0 + k).
  static IRAM_ATTR uint32_t pipe_asm(const uint32_t* in, uint32_t ns, uint32_t count) {
    const uint32_t sb = SHA_TEXT_BASE, ctl = SHA_TEXT_BASE + 0x90;  // [TRM] START=+0x90 CONTINUE=+0x94 LOAD=+0x98 BUSY=+0x9C
    const uint32_t pad = 0x80000000u, len1 = 0x280u, len2 = 0x100u, inc = 0x01000000u, zero = 0;
    uint32_t t, k = 0;
    __asm__ __volatile__(
    "1:\n"
      // bloco 1 (64 bytes constantes do job)
      "l32i %[t], %[in],  0\n s32i %[t], %[sb],  0\n"  "l32i %[t], %[in],  4\n s32i %[t], %[sb],  4\n"
      "l32i %[t], %[in],  8\n s32i %[t], %[sb],  8\n"  "l32i %[t], %[in], 12\n s32i %[t], %[sb], 12\n"
      "l32i %[t], %[in], 16\n s32i %[t], %[sb], 16\n"  "l32i %[t], %[in], 20\n s32i %[t], %[sb], 20\n"
      "l32i %[t], %[in], 24\n s32i %[t], %[sb], 24\n"  "l32i %[t], %[in], 28\n s32i %[t], %[sb], 28\n"
      "l32i %[t], %[in], 32\n s32i %[t], %[sb], 32\n"  "l32i %[t], %[in], 36\n s32i %[t], %[sb], 36\n"
      "l32i %[t], %[in], 40\n s32i %[t], %[sb], 40\n"  "l32i %[t], %[in], 44\n s32i %[t], %[sb], 44\n"
      "l32i %[t], %[in], 48\n s32i %[t], %[sb], 48\n"  "l32i %[t], %[in], 52\n s32i %[t], %[sb], 52\n"
      "l32i %[t], %[in], 56\n s32i %[t], %[sb], 56\n"  "l32i %[t], %[in], 60\n s32i %[t], %[sb], 60\n"
      "movi %[t], 1\n s32i %[t], %[ctl], 0\n memw\n"                       // START
      // pipeline [SM]: bloco 2 entra enquanto o motor processa o bloco 1
      "l32i %[t], %[in], 64\n s32i %[t], %[sb], 0\n"
      "l32i %[t], %[in], 68\n s32i %[t], %[sb], 4\n"
      "l32i %[t], %[in], 72\n s32i %[t], %[sb], 8\n"
      "s32i %[ns], %[sb], 12\n"
      "s32i %[pad], %[sb], 16\n"
      "s32i %[z], %[sb], 20\n s32i %[z], %[sb], 24\n s32i %[z], %[sb], 28\n s32i %[z], %[sb], 32\n s32i %[z], %[sb], 36\n"
      "s32i %[z], %[sb], 40\n s32i %[z], %[sb], 44\n s32i %[z], %[sb], 48\n s32i %[z], %[sb], 52\n s32i %[z], %[sb], 56\n"
      "s32i %[len1], %[sb], 60\n"
    "2: l32i %[t], %[ctl], 12\n bnez %[t], 2b\n"                            // BUSY
      "movi %[t], 1\n s32i %[t], %[ctl], 4\n memw\n"                       // CONTINUE
    "3: l32i %[t], %[ctl], 12\n bnez %[t], 3b\n"
      "movi %[t], 1\n s32i %[t], %[ctl], 8\n memw\n"                       // LOAD (digest → TEXT)
    "4: l32i %[t], %[ctl], 12\n bnez %[t], 4b\n"
      // 2º SHA: digest já em TEXT[0..7]; TEXT[9..14] continuam zero [SM]
      "s32i %[pad], %[sb], 32\n s32i %[len2], %[sb], 60\n"
      "movi %[t], 1\n s32i %[t], %[ctl], 0\n memw\n"                       // START
    "5: l32i %[t], %[ctl], 12\n bnez %[t], 5b\n"
      "movi %[t], 1\n s32i %[t], %[ctl], 8\n memw\n"                       // LOAD
    "6: l32i %[t], %[ctl], 12\n bnez %[t], 6b\n"
      "l16ui %[t], %[sb], 28\n"                                             // early-reject 16 bits [SM]
      "beqz %[t], 9f\n"
      "addi %[k], %[k], 1\n"
      "add %[ns], %[ns], %[inc]\n"
      "bne %[k], %[cnt], 1b\n"
    "9:\n"
      : [t]"=&r"(t), [k]"+&r"(k), [ns]"+&r"(ns)  // "&": k começa em 0 == [z]; sem early-clobber o GCC pode dar o mesmo registrador aos dois
      : [sb]"r"(sb), [ctl]"r"(ctl), [in]"r"(in), [cnt]"r"(count), [pad]"r"(pad),
        [len1]"r"(len1), [len2]"r"(len2), [inc]"r"(inc), [z]"r"(zero)
      : "memory");
    return k;
  }
#ifndef SHA_PIPE_C
  IRAM_ATTR bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) override {
    bool hit = false;
    uint32_t n = nonce;
    esp_sha_lock_engine(SHA2_256);
    while (count) {
      uint32_t sub = 256 - (n & 0xFF);          // sub-bloco alinhado: só o byte baixo do nonce muda
      if (sub > count) sub = count;
      uint32_t k = pipe_asm(w, __builtin_bswap32(n), sub);
      n += k; count -= k;
      if (k < sub) {                             // acerto de 16 bits em n
        DPORT_INTERRUPT_DISABLE();
        for (int i = 0; i < 8; ++i) ((uint32_t*)hash)[i] = __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + i * 4));
        DPORT_INTERRUPT_RESTORE();
        hit = true;
        break;
      }
    }
    esp_sha_unlock_engine(SHA2_256);
    nonce = n;
    return hit;
  }
#else  // -D SHA_PIPE_C: laço em C (v5), mantido para A/B — ~30 % mais lento que o asm
  IRAM_ATTR bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) override {
    uint32_t* T = (uint32_t*)SHA_TEXT_BASE;  // stores simples (s32i), sem memw por palavra [SM]
    // não-volatile: -mserialize-volatile poria memw antes E depois de cada store; o MEMW() explícito basta [SM]
    uint32_t* START = (uint32_t*)SHA_256_START_REG;
    uint32_t* CONT  = (uint32_t*)SHA_256_CONTINUE_REG;
    uint32_t* LOAD  = (uint32_t*)SHA_256_LOAD_REG;
    bool hit = false;
    uint32_t n = nonce;  // local: a referência forçava load/store por iteração (clobbers "memory")
    const uint32_t* w = this->w;
    // ponytail: lock por chamada (lotes de 2048 nonces, ~4 ms); mover para o job se ainda aparecer no perfil
    esp_sha_lock_engine(SHA2_256);
    for (; count; --count, ++n) {
      // bloco 1 (64 bytes constantes do job)
      T[0] = w[0];  T[1] = w[1];  T[2] = w[2];  T[3] = w[3];  T[4] = w[4];  T[5] = w[5];  T[6] = w[6];  T[7] = w[7];
      T[8] = w[8];  T[9] = w[9];  T[10] = w[10]; T[11] = w[11]; T[12] = w[12]; T[13] = w[13]; T[14] = w[14]; T[15] = w[15];
      BARRIER(); *START = 1; MEMW();
      // pipeline [SM]: bloco 2 entra enquanto o motor processa o bloco 1
      T[0] = w[16]; T[1] = w[17]; T[2] = w[18]; T[3] = __builtin_bswap32(n);
      T[4] = 0x80000000u; T[5] = 0; T[6] = 0; T[7] = 0; T[8] = 0; T[9] = 0; T[10] = 0;
      T[11] = 0; T[12] = 0; T[13] = 0; T[14] = 0; T[15] = 0x00000280u;  // 640 bits
      BARRIER(); wait_idle(); *CONT = 1; MEMW();
      wait_idle(); *LOAD = 1; MEMW(); wait_idle();
      // 2º SHA: digest já está em T[0..7]; T[9..14] continuam zero do bloco 2 [SM]
      T[8] = 0x80000000u; T[15] = 0x00000100u;  // 256 bits
      BARRIER(); *START = 1; MEMW();
      wait_idle(); *LOAD = 1; MEMW(); wait_idle();
      // early-reject (§4.3 técnica 3): metade baixa da word 7 == bytes 30..31 do hash
      if ((SHA_RD(SHA_TEXT_BASE + 7 * 4) & 0xFFFF) == 0) {
        DPORT_INTERRUPT_DISABLE();  // leitura DPORT sequenciada, como no minerWorkerHw original
        for (int i = 0; i < 8; ++i) ((uint32_t*)hash)[i] = __builtin_bswap32(DPORT_SEQUENCE_REG_READ(SHA_TEXT_BASE + i * 4));
        DPORT_INTERRUPT_RESTORE();
        hit = true;
        break;
      }
    }
    esp_sha_unlock_engine(SHA2_256);
    nonce = n;
    return hit;
  }
#endif
};
HwPipelineEsp32 s_pipeline;
}
IShaBackend* sha_backend_hw_pipeline() { return &s_pipeline; }

#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#include <soc/soc.h>
#include <hal/sha_ll.h>
#include <hal/sha_hal.h>
#include <sha/sha_dma.h>

namespace {
static inline void wait_idle() { while (REG_READ(SHA_BUSY_REG)) {} }

// Variante S3/C3: o motor tem SHA_H_n graváveis → midstate injetado por nonce, 2 compressões.
// Sequência herdada do minerWorkerHw S3 do NerdMiner_v2 (GPL-3.0), já validada em campo.
struct HwPipelineS3 : IShaBackend {
  uint32_t mid[8], tail[3];
  const char* name() const override { return "hw-pipeline"; }
  void prepare(const uint8_t h[128]) override {
    esp_sha_acquire_hardware();
    sha_hal_hash_block(SHA2_256, h, 64 / 4, true);
    sha_hal_read_digest(SHA2_256, mid);
    esp_sha_release_hardware();
    memcpy(tail, h + 64, sizeof(tail));
  }
  IRAM_ATTR bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) override {
    volatile uint32_t* T = (volatile uint32_t*)SHA_TEXT_BASE;
    volatile uint32_t* H = (volatile uint32_t*)SHA_H_BASE;
    bool hit = false;
    esp_sha_acquire_hardware();
    REG_WRITE(SHA_MODE_REG, SHA2_256);
    for (; count; --count, ++nonce) {
      for (int i = 0; i < 8; ++i) H[i] = mid[i];
      T[0] = tail[0]; T[1] = tail[1]; T[2] = tail[2]; T[3] = nonce;
      T[4] = 0x00000080u; T[5] = 0; T[6] = 0; T[7] = 0; T[8] = 0; T[9] = 0; T[10] = 0;
      T[11] = 0; T[12] = 0; T[13] = 0; T[14] = 0; T[15] = 0x80020000u;
      REG_WRITE(SHA_CONTINUE_REG, 1);
      sha_ll_load(SHA2_256);
      wait_idle();
      for (int i = 0; i < 8; ++i) T[i] = H[i];
      T[8] = 0x00000080u; T[9] = 0; T[10] = 0; T[11] = 0; T[12] = 0; T[13] = 0; T[14] = 0; T[15] = 0x00010000u;
      REG_WRITE(SHA_START_REG, 1);
      sha_ll_load(SHA2_256);
      wait_idle();
      uint32_t last = H[7];
      if ((uint16_t)(last >> 16) == 0) {
        for (int i = 0; i < 7; ++i) ((uint32_t*)hash)[i] = H[i];
        ((uint32_t*)hash)[7] = last;
        hit = true;
        break;
      }
    }
    esp_sha_release_hardware();
    return hit;
  }
};
HwPipelineS3 s_pipeline;
}
IShaBackend* sha_backend_hw_pipeline() { return &s_pipeline; }

#else
IShaBackend* sha_backend_hw_pipeline() { return nullptr; }
#endif
#endif  // !HOST_TEST

// ---------------- selftest + bench + seleção ----------------
static void pad_header(uint8_t out[128], const uint8_t h80[80]) {
  memcpy(out, h80, 80);
  memset(out + 80, 0, 48);
  out[80] = 0x80; out[126] = 0x02; out[127] = 0x80;
}

bool sha_backend_selftest(IShaBackend& b) {
  uint8_t h[128], hash[32];
  for (const auto& v : SHA_VECTORS) {
    pad_header(h, v.header);
    uint32_t nonce, want; memcpy(&want, v.header + 76, 4);
    nonce = want;
    b.prepare(h);
    if (!b.scan(nonce, 1, hash) || nonce != want || memcmp(hash, v.hash, 32) != 0) return false;
  }
  return true;
}

#ifndef HOST_TEST
float sha_backend_khs(IShaBackend& b, uint32_t ms) {
  uint8_t h[128], hash[32];
  pad_header(h, SHA_VECTORS[1].header);
  b.prepare(h);
  uint32_t n0 = 0x10000000, n = n0, t0 = micros(), dt;
  do {
    if (b.scan(n, 2048, hash)) ++n;  // acerto de 16 bits (1/65536): retoma após o candidato
    dt = micros() - t0;
  } while (dt < ms * 1000);
  return dt ? (float)(n - n0) * 1000.0f / (float)dt : 0.0f;
}

ShaBench sha_bench = {};

// Cross-check de lote (§10 passo 3, complemento): varre `ms` ms com header aleatório e confere cada acerto de
// 16 bits contra mbedtls (double-SHA do header com o nonce devolvido). Pega erro de mapeamento de nonce/índice
// que os 8 vetores (acerto em k=0) não pegam. Devolve acertos verificados; imprime cada divergência.
static int crosscheck(IShaBackend& b, uint32_t ms, int& bad) {
  uint8_t h[128], hash[32], ref[32], inter[32];
  uint32_t rnd = 0x1234567;
  for (int i = 0; i < 80; ++i) { rnd = rnd * 1664525u + 1013904223u; h[i] = (uint8_t)(rnd >> 24); }
  memset(h + 80, 0, 48); h[80] = 0x80; h[126] = 0x02; h[127] = 0x80;
  b.prepare(h);
  uint32_t n = 0xDA54E700, t0 = millis();
  int ok = 0; bad = 0;
  while (millis() - t0 < ms) {
    uint32_t n0 = n;
    if (!b.scan(n, 2048, hash)) continue;
    memcpy(h + 76, &n, 4);
    mbedtls_sha256_ret(h, 80, inter, 0); mbedtls_sha256_ret(inter, 32, ref, 0);
    if (memcmp(hash, ref, 32) == 0) ok++;
    else {
      bad++;
      if (bad <= 3) {
        Serial.printf("[CH] crosscheck %s: n0=%08x n=%08x k=%u\n  got ", b.name(), n0, n, n - n0);
        for (int i = 0; i < 32; ++i) Serial.printf("%02x", hash[i]);
        Serial.print("\n  ref ");
        for (int i = 0; i < 32; ++i) Serial.printf("%02x", ref[i]);
        Serial.println();
      }
    }
    ++n;
  }
  return ok;
}

static void bench_one(IShaBackend* b, ShaBenchEntry& e) {
  e.name = b ? b->name() : "none";
  e.ok = b && sha_backend_selftest(*b);
  e.khs = e.ok ? sha_backend_khs(*b, 250) : 0.0f;
}

void sha_backend_boot() {
  setCpuFrequencyMhz(240);  // sdkconfig do Arduino default é 160; o laço HW escala com o clock da CPU
  IShaBackend* pipe = sha_backend_hw_pipeline();
  bench_one(&sha_backend_sw(), sha_bench.sw);
  bench_one(&sha_backend_hw_baseline(), sha_bench.baseline);
  bench_one(pipe, sha_bench.pipeline);

  const char* want = SHA_BACKEND;
  IShaBackend* hw;
  if (strcmp(want, "pipeline") == 0 && sha_bench.pipeline.ok) hw = pipe;
  else if (strcmp(want, "sw") != 0 && sha_bench.baseline.ok) hw = &sha_backend_hw_baseline();
  else hw = &sha_backend_sw();
  sha_bench.hw = hw;
  sha_bench.sw_sel = &sha_backend_sw();

  char msg[200];
  snprintf(msg, sizeof(msg), "SHA bench: sw %.1f kH/s %s | hw-baseline %.1f kH/s %s | hw-pipeline %.1f kH/s %s -> hw worker=%s (SHA_BACKEND=%s)",
           sha_bench.sw.khs, sha_bench.sw.ok ? "ok" : "FAIL",
           sha_bench.baseline.khs, sha_bench.baseline.ok ? "ok" : "FAIL",
           sha_bench.pipeline.khs, pipe ? (sha_bench.pipeline.ok ? "ok" : "FAIL") : "n/a",
           hw->name(), want);
  Serial.printf("[CH] %s cpu=%uMHz\n", msg, (unsigned)getCpuFrequencyMhz());
  if (pipe && sha_bench.pipeline.ok) {
    int bad = 0, ok = crosscheck(*pipe, 1500, bad);
    Serial.printf("[CH] crosscheck hw-pipeline: %d acertos ok, %d divergentes\n", ok, bad);
    if (bad) { sha_bench.pipeline.ok = false; hw = sha_bench.baseline.ok ? &sha_backend_hw_baseline() : &sha_backend_sw(); sha_bench.hw = hw; Serial.printf("[CH] *** hw-pipeline reprovado no cross-check -> fallback %s ***\n", hw->name()); }
  }
  if (!sha_bench.sw.ok) Serial.println("[CH] *** SwBackend falhou nos vetores — mineração vai produzir shares inválidos ***");
}
#endif  // !HOST_TEST

// CriptoHost NerdOS — §4.3 / §10 passo 1: interface única para os backends SHA-256d.
#pragma once
#include <stdint.h>

// Contrato:
//  prepare(h): header de 80 bytes + padding SHA (buffer de 128 bytes, layout do NerdMiner). 1x por job.
//  scan(nonce, count, hash): varre `count` nonces a partir de `nonce`. Se um hash tiver bytes 30..31 == 0
//    (early-reject de 16 bits — é o que a pool exige no mínimo), devolve true com `nonce` = candidato e
//    `hash` completo; senão devolve false com `nonce += count`. O chamador decide dificuldade/submit.
struct IShaBackend {
  virtual const char* name() const = 0;
  virtual void prepare(const uint8_t header128[128]) = 0;
  virtual bool scan(uint32_t& nonce, uint32_t count, uint8_t hash[32]) = 0;
  virtual ~IShaBackend() {}
};

IShaBackend& sha_backend_sw();              // nerdSHA256plus (midstate + bake) — caminho original do NerdMiner
IShaBackend& sha_backend_hw_baseline();     // mbedTLS (periférico via CONFIG_MBEDTLS_HARDWARE_SHA)
IShaBackend* sha_backend_hw_pipeline();     // registradores diretos + pipelining; nullptr se o chip não tem variante

bool  sha_backend_selftest(IShaBackend& b);            // 8 vetores conhecidos (§10 passo 3)
float sha_backend_khs(IShaBackend& b, uint32_t ms);    // bench isolado, kH/s de double-SHA por nonce

struct ShaBenchEntry { const char* name; float khs; bool ok; };
struct ShaBench {
  ShaBenchEntry sw, baseline, pipeline;
  IShaBackend* hw;     // backend escolhido para o worker HW (core 0)
  IShaBackend* sw_sel; // backend do worker SW (core 1)
};
extern ShaBench sha_bench;

// Roda selftest + bench nos 3 backends e escolhe hw/sw. SHA_BACKEND ("pipeline" default | "baseline" | "sw")
// só é honrado se passar no selftest; senão cai para baseline → sw com log (§10 passo 3).
void sha_backend_boot();

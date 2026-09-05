// Check único do SwBackend no host: os 8 vetores de src/mining/sha_vectors.h.
// Uso: sh test/host/run.sh
#include "mining/sha_backend.h"
#include <assert.h>
#include <stdio.h>
int main() {
  assert(sha_backend_selftest(sha_backend_sw()));
  puts("OK — SwBackend reproduz os 8 vetores");
  return 0;
}

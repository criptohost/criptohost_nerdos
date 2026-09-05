#!/usr/bin/env python3
"""Suite de vetores SHA-256d (M1-01/M1-02/M1-13: todos os backends devem reproduzir o hash).

Gera/valida test/sha_vectors.json e src/mining/sha_vectors.h: 8 headers de 80 bytes cujo
double-SHA256 termina em 16 bits zero (passam o early-reject do pipeline HW, então
IShaBackend::scan devolve o hash completo). O firmware roda esses 8 no boot (§10 passo 3).
Uso: python3 test/test_sha_vectors.py [--gen]
"""
import hashlib, json, os, struct, sys, random

HERE = os.path.dirname(os.path.abspath(__file__))
VECTORS = os.path.join(HERE, "sha_vectors.json")
HEADER_C = os.path.join(HERE, "..", "src", "mining", "sha_vectors.h")
N = 8


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def gen():
    genesis = bytes.fromhex(
        "0100000000000000000000000000000000000000000000000000000000000000"
        "000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa"
        "4b1e5e4a29ab5f49ffff001d1dac2b7c"
    )
    vecs = [genesis]  # nonce real do génesis: 32 bits finais zerados
    rnd = random.Random(0x43480001)
    while len(vecs) < N:
        base = bytes(rnd.getrandbits(8) for _ in range(76))
        nonce = 0
        while sha256d(base + struct.pack("<I", nonce))[30:] != b"\x00\x00":
            nonce += 1
        vecs.append(base + struct.pack("<I", nonce))
    out = [{"header": v.hex(), "sha256d": sha256d(v).hex()} for v in vecs]
    with open(VECTORS, "w") as f:
        json.dump(out, f, indent=2)
    os.makedirs(os.path.dirname(HEADER_C), exist_ok=True)
    with open(HEADER_C, "w") as f:
        f.write("// Gerado por test/test_sha_vectors.py — não editar à mão.\n")
        f.write("// 8 vetores header(80 bytes) -> sha256d, todos com bytes 30..31 == 0 (early-reject 16 bits).\n")
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write("static const struct { uint8_t header[80]; uint8_t hash[32]; } SHA_VECTORS[%d] = {\n" % N)
        for v in vecs:
            f.write("  {{" + ",".join("0x%02x" % b for b in v) + "},\n   {" + ",".join("0x%02x" % b for b in sha256d(v)) + "}},\n")
        f.write("};\n")
    return out


def main():
    vecs = gen() if ("--gen" in sys.argv or not os.path.exists(VECTORS)) else json.load(open(VECTORS))
    assert len(vecs) == N
    for i, v in enumerate(vecs):
        h = bytes.fromhex(v["header"])
        assert len(h) == 80, f"vetor {i}: header != 80 bytes"
        got = sha256d(h)
        assert got.hex() == v["sha256d"], f"vetor {i}: esperado {v['sha256d']}, obtido {got.hex()}"
        assert got[30:] == b"\x00\x00", f"vetor {i}: precisa terminar em 16 bits zero"
    print(f"OK — {len(vecs)} vetores validados ({VECTORS})")


if __name__ == "__main__":
    main()

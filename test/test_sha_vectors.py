#!/usr/bin/env python3
"""Suite de vetores SHA-256d (M1-01/M1-02: HW e SW devem produzir resultados idênticos).

Gera/valida test/sha_vectors.json: headers de 80 bytes -> double-SHA256 esperado.
O firmware consome esses vetores no teste serial (ShaTests); este script é o oráculo host.
"""
import hashlib, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
VECTORS = os.path.join(HERE, "sha_vectors.json")


def sha256d(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def gen():
    vecs = []
    # header real do bloco génesis do Bitcoin + variações determinísticas de nonce
    genesis = bytes.fromhex(
        "0100000000000000000000000000000000000000000000000000000000000000"
        "000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa"
        "4b1e5e4a29ab5f49ffff001d1dac2b7c"
    )
    assert len(genesis) == 80
    for nonce in (0x7C2BAC1D, 0, 1, 0xFFFFFFFF, 0xDEADBEEF):  # 0x7C2BAC1D = nonce real do génesis
        h = bytearray(genesis)
        h[76:80] = nonce.to_bytes(4, "little")
        vecs.append({"header": bytes(h).hex(), "sha256d": sha256d(bytes(h)).hex()})
    with open(VECTORS, "w") as f:
        json.dump(vecs, f, indent=2)
    return vecs


def main():
    vecs = gen() if not os.path.exists(VECTORS) else json.load(open(VECTORS))
    for i, v in enumerate(vecs):
        h = bytes.fromhex(v["header"])
        assert len(h) == 80, f"vetor {i}: header != 80 bytes"
        got = sha256d(h).hex()
        assert got == v["sha256d"], f"vetor {i}: esperado {v['sha256d']}, obtido {got}"
    # sanity: hash do génesis com nonce original termina nos zeros conhecidos
    g = bytes.fromhex(vecs[0]["sha256d"])
    assert g[28:] == b"\x00\x00\x00\x00", "génesis deveria ter 32 bits finais zerados"
    print(f"OK — {len(vecs)} vetores validados ({VECTORS})")


if __name__ == "__main__":
    main()

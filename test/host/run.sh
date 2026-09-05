#!/bin/sh
set -e
cd "$(dirname "$0")/../.."
python3 test/test_sha_vectors.py
c++ -std=c++17 -O2 -DHOST_TEST -include test/host/stubs/Arduino.h -I test/host/stubs -I src \
  src/mining/sha_backend.cpp src/ShaTests/nerdSHA256plus.cpp test/host/selftest_host.cpp \
  -o "${TMPDIR:-/tmp}/sha_selftest_host" && "${TMPDIR:-/tmp}/sha_selftest_host"

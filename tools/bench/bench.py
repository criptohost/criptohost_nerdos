#!/usr/bin/env python3
"""§10 passo 4 — lê a serial do nó por N s e reporta média/mín/máx de kH/s.

O firmware imprime 1x/s:  [BENCH] khs=<total> hw=<backend> sw=<backend>
e no boot:                [CH] SHA bench: sw X kH/s ok | hw-baseline Y ... | hw-pipeline Z ...
O total é HW (core 0) + SW (core 1); o isolado por backend é a linha de boot.
Para medir outro backend, reflashe com -D SHA_BACKEND='"baseline"' (ou "sw").

Uso: python3 tools/bench/bench.py /dev/cu.usbserial-0001 --seconds 60 --runs 3 --label "wifi on"
(pyserial vem no venv do PlatformIO: ~/.platformio/penv/bin/python)
"""
import argparse, re, statistics, sys, time
import serial

LINE = re.compile(r"\[BENCH\] khs=(\d+) hw=(\S+) sw=(\S+)")
BOOT = re.compile(r"SHA bench: .*")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--label", default="")
    ap.add_argument("--reset", action="store_true", help="reseta a placa (DTR/RTS) antes de ler, para capturar a linha de boot")
    a = ap.parse_args()

    ser = serial.Serial(a.port, a.baud, timeout=1)
    if a.reset:
        ser.setDTR(False); ser.setRTS(True); time.sleep(0.1); ser.setRTS(False)
    print("| label | hw | sw | run | média kH/s | mín | máx | amostras |")
    print("|---|---|---|---|---|---|---|---|")
    for run in range(1, a.runs + 1):
        samples, hw, sw = [], "?", "?"
        deadline = time.time() + a.seconds
        while time.time() < deadline:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue
            m = LINE.search(line)
            if m:
                samples.append(int(m.group(1))); hw, sw = m.group(2), m.group(3)
            elif BOOT.search(line):
                print("# boot:", BOOT.search(line).group(0), file=sys.stderr)
        if not samples:
            print(f"| {a.label} | ? | ? | {run} | sem amostras (linha [BENCH] não vista em {a.seconds}s) | | | 0 |")
            continue
        print(f"| {a.label} | {hw} | {sw} | {run} | {statistics.mean(samples):.1f} | {min(samples)} | {max(samples)} | {len(samples)} |")


if __name__ == "__main__":
    main()

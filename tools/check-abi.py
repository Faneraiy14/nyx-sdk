#!/usr/bin/env python3
"""tools/check-abi.py — чи має ядро всі syscall'и, яких потребує програма (Р5).

Програма (через programs/libnyx.h) несе в ELF секції .nyxabi (номери потрібних викликів, u32) і .nyxabi_ver
(версія ABI SDK). Порівнюємо з abi.txt ядра (tools/gen-abi.py). Бракує виклику або інша версія ABI -> код 1 і
пояснення: збірка блокується, програма не потрапить в образ.
Використання: tools/check-abi.py <abi.txt> <програма.elf> [<програма.elf> ...]
"""
import os
import struct
import subprocess
import sys
import tempfile

abi_path, elfs = sys.argv[1], sys.argv[2:]
lines = open(abi_path, encoding='utf-8').read().split('\n')
abi_ver = int(lines[0].split()[1])
core = lines[1].split()[1]
calls = {}
for l in lines[2:]:
    if l.strip():
        n, name = l.split()
        calls[int(n)] = name


def section(elf, name):
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, 's.bin')
        r = subprocess.run(['objcopy', f'--dump-section={name}={out}', elf, os.path.join(d, 'x.elf')],
                           capture_output=True)
        if r.returncode != 0 or not os.path.exists(out):
            return []
        data = open(out, 'rb').read()
    return list(struct.unpack(f'<{len(data) // 4}I', data[:len(data) // 4 * 4]))


bad = 0
for elf in elfs:
    prog = os.path.basename(elf).rsplit('.', 1)[0]
    ver = section(elf, '.nyxabi_ver')
    need = sorted(set(section(elf, '.nyxabi')))
    if not ver:
        print(f'❌ {prog}: нема .nyxabi_ver - зібрано не з programs/libnyx.h цього SDK')
        bad += 1
        continue
    if ver[0] != abi_ver:
        print(f'❌ {prog}: зібрано під ABI {ver[0]}, а ядро NyxCore {core} має ABI {abi_ver}')
        bad += 1
    missing = [n for n in need if n not in calls]
    if missing:
        print(f'❌ {prog}: ядро NyxCore {core} (ABI {abi_ver}) не має syscall\'ів {", ".join(map(str, missing))}')
        bad += 1
sys.exit(1 if bad else 0)

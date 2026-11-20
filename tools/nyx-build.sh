#!/usr/bin/env bash
# tools/nyx-build.sh — збирає програму NyxOS мовою C (публічний SDK NyxCore).
#   tools/nyx-build.sh <тека-виходу> <програма.c> [<програма.c> ...]
# Для кожної: <тека>/<ім'я>.elf і <ім'я>.bin — сирий бінарник для NyxOS (кладеш у /bin чи /games).
# Перевірка ABI: кожен системний виклик, якого потребує програма, має бути в abi.txt цього SDK (ядро тієї ж версії);
# бракує — збірка зупиняється. Потрібно: gcc (з -m32), binutils, python3.
set -e
SDK="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$1"
shift || true
[ -n "$OUT" ] && [ $# -gt 0 ] || { echo "використання: $0 <тека-виходу> <програма.c>..."; exit 2; }
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
CFLAGS="-m32 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -O0 -fno-toplevel-reorder -Wall -Wextra"
as --32 "$SDK/lib/start.s" -o "$OUT/start.o"
ELFS=()
for src in "$@"; do
    name="$(basename "${src%.c}")"
    gcc $CFLAGS -I"$SDK/include" -c "$src" -o "$OUT/$name.o"
    ld -m elf_i386 -T "$SDK/lib/user.ld" -o "$OUT/$name.elf" "$OUT/start.o" "$OUT/$name.o" 2> >(grep -v "GNU-stack\|RWX\|NOTE: This behaviour" >&2)
    objcopy -O binary "$OUT/$name.elf" "$OUT/$name.bin"
    echo "  $name.bin ($(wc -c < "$OUT/$name.bin") байтів)"
    ELFS+=("$OUT/$name.elf")
done
python3 "$SDK/tools/check-abi.py" "$SDK/abi.txt" "${ELFS[@]}" || { echo "❌ перевірка ABI: збірку заблоковано"; exit 1; }
echo "  ABI: сумісно з $(sed -n 2p "$SDK/abi.txt") (ABI $(sed -n 1p "$SDK/abi.txt" | cut -d' ' -f2))"

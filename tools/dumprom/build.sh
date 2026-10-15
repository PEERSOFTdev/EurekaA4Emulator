#!/usr/bin/env bash
# Assembles DUMPROM.COM from dumprom.s with GNU binutils for Z80
# (Ubuntu/Debian: apt install binutils-z80).  The COM is committed, so only
# whoever changes the source needs this.
set -euo pipefail
cd "$(dirname "$0")"

prefix=${Z80PREFIX:-z80-unknown-coff-}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"${prefix}as" -march=z180 -o "$work/dumprom.o" dumprom.s
# A CP/M program is loaded at 0100h.
"${prefix}ld" -Ttext=0x100 -o "$work/dumprom.elf" "$work/dumprom.o"
"${prefix}objcopy" -O binary "$work/dumprom.elf" DUMPROM.COM

# The buffer at 2000h (BUF in dumprom.s) must lie above the program.
size=$(stat -c %s DUMPROM.COM)
if (( 0x100 + size > 0x2000 )); then
  echo "DUMPROM.COM has $size bytes and reaches into BUF at 2000h" >&2
  exit 1
fi
echo "DUMPROM.COM: $size bytes"

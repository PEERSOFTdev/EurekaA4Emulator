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

# DUMPROM.IMG: an 800 KiB Eureka diskette holding only DUMPROM.COM, as a raw
# image that SAMdisk writes to a real floppy.  Built here rather than by hand
# so that it always carries the COM next to it.
#
# The diskette (src/cpm_disk.h): 80 cylinders, 2 heads, 10 sectors of 512
# bytes; 2 KiB blocks, the directory in blocks 0-3, data from block 4.  A
# logical sector L lies in the sector with ID s where kLogical[s-1] = L
# (SectorOffset in src/virtual_disk.cpp), and track t is cylinder t/2, head
# t%2.  SAMdisk's raw image holds every track of head 0 first, then head 1,
# each track in sector ID order.
#
# One directory entry holds 16 KiB (src/cpm_disk.h, kRecordsPerEntry).
if (( size > 16384 )); then
  echo "DUMPROM.COM is over 16 KiB; DUMPROM.IMG has room for one extent" >&2
  exit 1
fi
records=$(( (size + 127) / 128 ))
blocks=$(( (size + 2047) / 2048 ))

head -c 819200 /dev/zero | tr '\0' '\345' > "$work/logical"
# The directory entry: user 0, DUMPROM.COM, extent 0, record count, then
# 16-bit block numbers 4, 5, ... and zeros.
{
  printf '\000DUMPROM COM\000\000\000'
  printf "\\$(printf %03o "$records")"
  for (( b = 0; b < 8; ++b )); do
    if (( b < blocks )); then printf "\\$(printf %03o $(( 4 + b )))\\000"
    else printf '\000\000'; fi
  done
} | dd of="$work/logical" conv=notrunc status=none
# The file from block 4: its last record padded with ^Z, the rest of the last
# block with zeros -- what CP/M tools write, and what CPMIMG wrote when this
# layout was checked against it.
{
  cat DUMPROM.COM
  head -c $(( records * 128 - size )) /dev/zero | tr '\0' '\032'
  head -c $(( blocks * 2048 - records * 128 )) /dev/zero
} | dd of="$work/logical" bs=2048 seek=4 conv=notrunc status=none

kLogical=(3 0 7 4 1 8 5 2 9 6)
: > DUMPROM.IMG
for head in 0 1; do
  for (( cylinder = 0; cylinder < 80; ++cylinder )); do
    track=$(( cylinder * 2 + head ))
    for (( id = 1; id <= 10; ++id )); do
      dd if="$work/logical" bs=512 skip=$(( track * 10 + kLogical[id - 1] )) \
         count=1 status=none >> DUMPROM.IMG
    done
  done
done
echo "DUMPROM.IMG: $(stat -c %s DUMPROM.IMG) bytes"

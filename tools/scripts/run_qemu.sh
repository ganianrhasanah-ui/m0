#!/usr/bin/env bash
set -euo pipefail

ISO="build/mcsos.iso"
LOG="build/qemu-serial.log"

mkdir -p build
rm -f "$LOG"

if [ ! -f "$ISO" ]; then
  echo "ERROR: ISO tidak ditemukan. jalankan make image" >&2
  exit 1
fi

timeout 10s qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -cdrom "$ISO" \
  -serial file:"$LOG" \
  -display none \
  -monitor none \
  -no-reboot \
  -no-shutdown || true

if [ ! -s "$LOG" ]; then
  echo "ERROR: serial log kosong (kernel belum boot atau serial tidak aktif)" >&2
  exit 1
fi

grep -q "MCSOS 260502 M3 kernel entered" "$LOG"
grep -q "\[M3\] selftest: basic invariants passed" "$LOG"
grep -q "\[M3\] ready for QEMU smoke test and GDB audit" "$LOG"

echo "OK: QEMU boot validation passed"

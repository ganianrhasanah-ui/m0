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

grep -q "MCSOS 260502 M2 boot path entered" "$LOG"
grep -q "\[M2\] early serial online" "$LOG"
grep -q "\[M2\] kernel reached controlled halt loop" "$LOG"

echo "OK: QEMU boot validation passed"

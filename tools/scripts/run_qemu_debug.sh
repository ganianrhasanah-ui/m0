#!/usr/bin/env bash
set -euo pipefail

ISO="build/mcsos.iso"

test -f "$ISO"

mkdir -p build

qemu-system-x86_64 \
  -machine q35 \
  -cpu qemu64 \
  -m 512M \
  -cdrom "$ISO" \
  -serial file:build/qemu-debug-serial.log \
  -display none \
  -monitor stdio \
  -no-reboot \
  -no-shutdown \
  -s -S

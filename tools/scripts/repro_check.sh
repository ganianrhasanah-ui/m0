#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/proof"

FIRST="$OUT/sha256-first.txt"
SECOND="$OUT/sha256-second.txt"

./tools/scripts/proof_compile.sh >/dev/null

sha256sum "$OUT/freestanding_probe.o" \
          "$OUT/freestanding_probe.elf" \
          > "$FIRST"

./tools/scripts/proof_compile.sh >/dev/null

sha256sum "$OUT/freestanding_probe.o" \
          "$OUT/freestanding_probe.elf" \
          > "$SECOND"

if diff -u "$FIRST" "$SECOND" > "$OUT/repro-diff.txt"; then
    echo "OK: reproducible build proof passed"
else
    echo "ERROR: reproducibility check failed" >&2
    exit 1
fi

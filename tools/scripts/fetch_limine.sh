#!/usr/bin/env bash
set -euo pipefail

LIMINE_DIR="third_party/limine"
LIMINE_URL="https://github.com/Limine-Bootloader/Limine.git"

mkdir -p third_party build/meta

if [ ! -d "$LIMINE_DIR" ]; then
  git clone "$LIMINE_URL" "$LIMINE_DIR"
fi

echo "Limine source ready (no build in M2)"


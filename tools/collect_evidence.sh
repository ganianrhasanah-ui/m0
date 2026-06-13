#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="build/evidence/M0"
mkdir -p "$TARGET_DIR"

echo "[*] Mengumpulkan metadata environment..."
cp build/meta/toolchain-versions.txt "$TARGET_DIR/" 2>/dev/null || true

echo "[*] Mengumpulkan bukti smoke test..."
cp build/smoke/readelf-header.txt "$TARGET_DIR/" 2>/dev/null || true
cp build/smoke/objdump.txt "$TARGET_DIR/" 2>/dev/null || true
cp build/smoke/file.txt "$TARGET_DIR/" 2>/dev/null || true

echo "[*] Mencatat ringkasan Git..."
git log --oneline -n 5 > "$TARGET_DIR/git_log_summary.txt"
git rev-parse HEAD > "$TARGET_DIR/git_commit_hash.txt"
git status --short > "$TARGET_DIR/git_status.txt"

echo "[✓] Semua bukti M0 berhasil dikumpulkan di $TARGET_DIR"


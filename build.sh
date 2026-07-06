#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Configuring (Release mode)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
NPROC=$(nproc 2>/dev/null || echo 1)
cmake --build build -j"$NPROC"

echo "==> Done. Binary: build/scx-switcher"
echo "    Run: sudo cmake --install build"

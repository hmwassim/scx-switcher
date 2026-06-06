#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Configuring..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
cmake --build build -j"$(nproc)"

echo "==> Done. Binary: build/scx-switcher"
echo "    Run: sudo cmake --install build"

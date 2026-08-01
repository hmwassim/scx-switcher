#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

PREFIX="${PREFIX:-/usr}"

echo "==> Configuring (Release mode, prefix=$PREFIX)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"

echo "==> Building..."
NPROC=$(nproc 2>/dev/null || echo 1)
cmake --build build -j"$NPROC"

echo "==> Done. Binary: build/scx-switcher"
echo "    Run: sudo cmake --install build"
echo "    (override prefix with: PREFIX=/usr/local ./build.sh)"

#!/usr/bin/env bash
# Build a .deb for scx-switcher.
# Requires: debhelper, cmake, qt6-base-dev, libgl-dev, g++ (>= 12)
#   sudo apt install debhelper cmake qt6-base-dev libgl-dev g++
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Building scx-switcher .deb…"
# -b: binary-only build (no source tarball — appropriate for local builds)
# -us -uc: skip signing
dpkg-buildpackage -us -uc -b

# dpkg-buildpackage writes output to the PARENT directory.
mkdir -p build
mv ../scx-switcher_*.deb          build/ 2>/dev/null || true
mv ../scx-switcher-dbgsym_*.deb   build/ 2>/dev/null || true

# Clean up the metadata files left in the parent directory.
rm -f ../scx-switcher_*.buildinfo \
      ../scx-switcher_*.changes   2>/dev/null || true

echo "==> Generating SHA256SUMS for all .deb files in build/…"
(cd build && sha256sum ./*.deb > SHA256SUMS)
echo "    $(wc -l < build/SHA256SUMS) checksum(s) written"

echo ""
echo "==> Done. Package(s) in build/:"
ls -lh build/scx-switcher_*.deb

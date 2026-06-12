#!/usr/bin/env bash
# scx-switcher — Update schedulers, loader, and GUI to latest release
set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────
REPO="https://github.com/hmwassim/scx-bundler"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="/tmp/scx-switcher-update.log"
DEB_DIR="/tmp/scx-switcher-update-debs"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo "amd64")}"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

for arg in "$@"; do
    case "$arg" in
        --repo=*) REPO="${arg#*=}" ;;
        --help|-h)
            echo "Usage: $0 [--repo=URL]"
            exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root."
fi

exec > >(tee -a "$LOG") 2>&1
log "=== scx-switcher update started ==="

# Detect latest release
OWNER_REPO="${REPO#https://github.com/}"
API="https://api.github.com/repos/$OWNER_REPO/releases/latest"
TAG=$(curl -sL "$API" | jq -r '.tag_name' 2>/dev/null || \
      curl -sL "$API" | grep -m1 '"tag_name"' | sed 's/.*"tag_name": "//;s/".*//')
if [ -z "$TAG" ]; then
    fail "Could not determine latest release from $REPO"
fi
VERSION="${TAG#v}"

echo ""
info "Updating SCX Switcher components to $TAG..."
echo ""

# Step 1: Download latest .debs
BASE_URL="$REPO/releases/download/$TAG"
info "Downloading scx-scheds..."
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR"
curl -sL "$BASE_URL/scx-scheds_${VERSION}_${ARCH}.deb" \
    -o "$DEB_DIR/scx-scheds_${VERSION}_${ARCH}.deb" || fail "scx-scheds download failed"
ok

info "Downloading scx-tools..."
curl -sL "$BASE_URL/scx-tools_${VERSION}_${ARCH}.deb" \
    -o "$DEB_DIR/scx-tools_${VERSION}_${ARCH}.deb" || fail "scx-tools download failed"
ok

# Step 2: Install .debs
info "Installing scx-scheds + scx-tools..."
sudo apt install -y "$DEB_DIR"/*.deb || fail "install failed"
ok

# Step 3: Rebuild GUI
info "Rebuilding SCX Switcher GUI..."
cd "$SCRIPT_DIR"
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release || fail "cmake configure failed"
cmake --build build -j "$(nproc)" || fail "cmake build failed"
sudo cmake --install build || fail "cmake install failed"
ok

rm -rf "$DEB_DIR"

echo ""
info "Update to $TAG complete."
info "Restart scx-switcher to apply GUI changes."
echo ""

log "=== update finished ==="

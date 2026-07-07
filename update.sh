#!/usr/bin/env bash
# scx-switcher — Update schedulers, loader, and GUI to latest release
set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────
REPO="https://github.com/hmwassim/scx-bundler"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo "amd64")}"

# Temp workspace (mktemp avoids predictable /tmp races on multi-user systems)
TMP_DIR=$(mktemp -d /tmp/scx-switcher-update.XXXXXX)
LOG="$TMP_DIR/update.log"
DEB_DIR="$TMP_DIR/debs"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
warn()  { echo -e "  ${YELLOW}WARN${NC} $*"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

cleanup() { [ -n "${TMP_DIR:-}" ] && rm -rf "$TMP_DIR"; }
trap cleanup EXIT

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

# Avoid set -e killing us on tee failure
exec > >(tee -a "$LOG") 2>&1 || true
log "=== scx-switcher update started ==="

# Resolve latest tag via HTTP redirect — no API calls (no rate limit)
LATEST_URL=$(curl -sIL -o /dev/null -w '%{url_effective}' "https://github.com/${REPO#https://github.com/}/releases/latest" 2>/dev/null || true)
TAG=$(basename "$LATEST_URL" 2>/dev/null || true)
if [ -z "$TAG" ] || [ "$TAG" = "latest" ]; then
    fail "Could not determine latest release from $REPO"
fi
VERSION="${TAG#v}"

verify_sha256() {
    (cd "$1" && grep "$2" SHA256SUMS | sha256sum -c -) >/dev/null 2>&1 || return 1
}

echo ""
info "Updating SCX Switcher components to $TAG..."
echo ""

# Step 1: Download latest .debs
BASE_URL="$REPO/releases/download/$TAG"
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR"
info "Downloading SHA256SUMS..."
if curl -sL --fail "$BASE_URL/SHA256SUMS" -o "$DEB_DIR/SHA256SUMS" 2>/dev/null; then
    ok
    HAS_SUMS=1
else
    warn "SHA256SUMS not found in release — skipping integrity verification"
    HAS_SUMS=0
fi

info "Downloading scx-scheds..."
DEB_SCHEDS="scx-scheds_${VERSION}_${ARCH}.deb"
curl -sL "$BASE_URL/$DEB_SCHEDS" \
    -o "$DEB_DIR/$DEB_SCHEDS" || fail "scx-scheds download failed"
if [ "$HAS_SUMS" = "1" ] && ! verify_sha256 "$DEB_DIR" "$DEB_SCHEDS"; then
    fail "scx-scheds SHA256 mismatch"
fi
ok

info "Downloading scx-tools..."
DEB_TOOLS="scx-tools_${VERSION}_${ARCH}.deb"
curl -sL "$BASE_URL/$DEB_TOOLS" \
    -o "$DEB_DIR/$DEB_TOOLS" || fail "scx-tools download failed"
if [ "$HAS_SUMS" = "1" ] && ! verify_sha256 "$DEB_DIR" "$DEB_TOOLS"; then
    fail "scx-tools SHA256 mismatch"
fi
ok

# Step 2: Install .debs
info "Installing scx-scheds + scx-tools..."
deb_files=("$DEB_DIR"/*.deb)
if [ ${#deb_files[@]} -eq 0 ] || [ ! -f "${deb_files[0]}" ]; then
    fail "No .deb files found in $DEB_DIR"
fi
sudo apt install -y "${deb_files[@]}" || fail "install failed"
ok

# Step 3: Rebuild GUI
info "Rebuilding SCX Switcher GUI..."
cd "$SCRIPT_DIR"
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr || fail "cmake configure failed"
cmake --build build -j "$(nproc)" || fail "cmake build failed"
sudo cmake --install build || fail "cmake install failed"
ok

# Step 4: Refresh systemd kernel-check drop-in
info "Installing systemd kernel-compatibility drop-in..."
sudo mkdir -p /etc/systemd/system/scx_loader.service.d
sudo cp "$SCRIPT_DIR/data/scx_loader-kernel-check.conf" \
    /etc/systemd/system/scx_loader.service.d/kernel-check.conf
sudo systemctl daemon-reload 2>/dev/null || true
ok

echo ""
info "Update to $TAG complete."
info "Restart scx-switcher to apply GUI changes."
echo ""

log "=== update finished ==="

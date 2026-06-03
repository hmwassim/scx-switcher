#!/usr/bin/env bash
# debforge-scx — Update schedulers, loader, and GUI
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="/tmp/debforge-scx-update"
export CARGO_HOME="$BUILD/cargo-home"
export RUSTUP_HOME="$BUILD/rustup-home"
LOG="/tmp/debforge-scx-update.log"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root."
fi

exec > >(tee -a "$LOG") 2>&1
log "=== debforge-scx update started ==="

echo ""
info "Updating DebForge SCX components..."
echo ""

# Bootstrap sandboxed Rust if not already present
if [ ! -f "$CARGO_HOME/bin/cargo" ]; then
    info "Installing Rust into sandbox ($CARGO_HOME)..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path
    . "$CARGO_HOME/env"
    rustup default stable
fi
export PATH="$CARGO_HOME/bin:$PATH"

# Step 1: Update scx schedulers
if command -v scxctl &>/dev/null; then
    info "Updating sched-ext schedulers..."
    rm -rf "$BUILD/scx"
    git clone --depth 1 "https://github.com/sched-ext/scx.git" "$BUILD/scx" || fail "scx clone failed"
    cargo build --release -j "$(nproc)" --manifest-path "$BUILD/scx/Cargo.toml" || fail "scx build failed"
    for f in "$BUILD/scx/target/release/scx_"*; do
        [ -x "$f" ] || continue
        sudo install -Dm755 "$f" /usr/bin/ || fail "install of $(basename "$f") failed"
    done
    ok
else
    info "scxctl not installed — skipping scheduler update"
fi

# Step 2: Update scx-loader
if command -v scxctl &>/dev/null; then
    info "Updating scx-loader..."
    rm -rf "$BUILD/loader"
    git clone --depth 1 "https://github.com/sched-ext/scx-loader.git" "$BUILD/loader" || fail "loader clone failed"
    cargo build --release -j "$(nproc)" --manifest-path "$BUILD/loader/Cargo.toml" || fail "loader build failed"
    sudo install -Dm755 "$BUILD/loader/target/release/scx_loader" /usr/bin/scx_loader
    sudo install -Dm755 "$BUILD/loader/target/release/scxctl" /usr/bin/scxctl
    ok
fi

# Step 3: Rebuild GUI
info "Rebuilding DebForge SCX GUI..."
cd "$SCRIPT_DIR"
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release || fail "cmake configure failed"
cmake --build build -j "$(nproc)" || fail "cmake build failed"
sudo cmake --install build || fail "cmake install failed"
ok

# Cleanup
rm -rf "$BUILD"

echo ""
info "Update complete."
info "Restart debforge-scx to apply GUI changes."
echo ""

log "=== update finished ==="

#!/usr/bin/env bash
# scx-switcher — Update schedulers, loader, and GUI
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="/tmp/scx-switcher-update"
LOG="/tmp/scx-switcher-update.log"

VERSIONS_FILE="/var/lib/scx-switcher/versions"
SCX_REPO="https://github.com/sched-ext/scx.git"
LOADER_REPO="https://github.com/sched-ext/scx-loader.git"

# Use debforge's shared Rust sandbox if available, else local ephemeral one
SHARED_CARGO="/opt/debforge/.cargo"
if [ -d "$SHARED_CARGO" ]; then
    export CARGO_HOME="$SHARED_CARGO"
    export RUSTUP_HOME="/opt/debforge/.rustup"
else
    export CARGO_HOME="$BUILD/cargo-home"
    export RUSTUP_HOME="$BUILD/rustup-home"
fi

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

load_versions() {
    SCX_COMMIT=""; SCX_LOADER_COMMIT=""
    [ -f "$VERSIONS_FILE" ] && source "$VERSIONS_FILE" 2>/dev/null || true
}

save_version() {
    local key="$1" val="$2"
    sudo mkdir -p "$(dirname "$VERSIONS_FILE")" 2>/dev/null
    if [ -f "$VERSIONS_FILE" ] && grep -q "^${key}=" "$VERSIONS_FILE" 2>/dev/null; then
        sudo sed -i "s|^${key}=.*|${key}=${val}|" "$VERSIONS_FILE"
    else
        echo "${key}=${val}" | sudo tee -a "$VERSIONS_FILE" >/dev/null
    fi
}

get_remote_head() {
    git ls-remote "$1" HEAD 2>/dev/null | awk '{print $1}'
}

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root."
fi

exec > >(tee -a "$LOG") 2>&1
log "=== scx-switcher update started ==="

echo ""
info "Updating SCX Switcher components..."
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
if command -v scx_rusty &>/dev/null || command -v scx_bpfland &>/dev/null; then
    load_versions
    SCX_REMOTE=$(get_remote_head "$SCX_REPO")
    if [ -n "$SCX_REMOTE" ] && [ "$SCX_REMOTE" = "$SCX_COMMIT" ]; then
        skip "sched-ext schedulers — up to date (commit ${SCX_COMMIT:0:12})"
    else
        info "Updating sched-ext schedulers..."
        rm -rf "$BUILD/scx"
        git clone --depth 1 "$SCX_REPO" "$BUILD/scx" || fail "scx clone failed"
        cargo build --release -j "$(nproc)" --manifest-path "$BUILD/scx/Cargo.toml" || fail "scx build failed"
        for f in "$BUILD/scx/target/release/scx_"*; do
            [ -x "$f" ] || continue
            sudo install -Dm755 "$f" /usr/bin/ || fail "install of $(basename "$f") failed"
        done
        if [ -n "$SCX_REMOTE" ]; then
            save_version "SCX_COMMIT" "$SCX_REMOTE"
        fi
        ok
    fi
else
    info "No scx schedulers installed — skipping scheduler update"
fi

# Step 2: Update scx-loader
if command -v scxctl &>/dev/null; then
    load_versions
    LOADER_REMOTE=$(get_remote_head "$LOADER_REPO")
    if [ -n "$LOADER_REMOTE" ] && [ "$LOADER_REMOTE" = "$SCX_LOADER_COMMIT" ]; then
        skip "scx-loader — up to date (commit ${SCX_LOADER_COMMIT:0:12})"
    else
        info "Updating scx-loader..."
        rm -rf "$BUILD/loader"
        git clone --depth 1 "$LOADER_REPO" "$BUILD/loader" || fail "loader clone failed"
        cargo build --release -j "$(nproc)" --manifest-path "$BUILD/loader/Cargo.toml" || fail "loader build failed"
        sudo install -Dm755 "$BUILD/loader/target/release/scx_loader" /usr/bin/scx_loader
        sudo install -Dm755 "$BUILD/loader/target/release/scxctl" /usr/bin/scxctl
        if [ -n "$LOADER_REMOTE" ]; then
            save_version "SCX_LOADER_COMMIT" "$LOADER_REMOTE"
        fi
        ok
    fi
fi

# Step 3: Rebuild GUI
info "Rebuilding SCX Switcher GUI..."
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
info "Restart scx-switcher to apply GUI changes."
echo ""

log "=== update finished ==="

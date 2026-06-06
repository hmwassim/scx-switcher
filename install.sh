#!/usr/bin/env bash
# scx-switcher — Full installation of sched-ext schedulers + GUI
# Usage: ./install.sh [--resume] [--skip-schedulers] [--skip-loader] [--skip-gui]
set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────
SCX_REPO="https://github.com/sched-ext/scx.git"
LOADER_REPO="https://github.com/sched-ext/scx-loader.git"
BUILD="/tmp/scx-switcher-build"

# Use debforge's shared Rust sandbox if available, else local ephemeral one
SHARED_CARGO="/opt/debforge/.cargo"
if [ -d "$SHARED_CARGO" ]; then
    export CARGO_HOME="$SHARED_CARGO"
    export RUSTUP_HOME="/opt/debforge/.rustup"
else
    export CARGO_HOME="$BUILD/cargo-home"
    export RUSTUP_HOME="$BUILD/rustup-home"
fi
LOG="/tmp/scx-switcher-install.log"
STATE_DIR="/tmp/scx-switcher-state"
STEP_FILE="$STATE_DIR/completed_steps"
LOCK_FILE="/tmp/scx-switcher-install.lock"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

FLAG_RESUME=false
FLAG_SKIP_SCHED=false
FLAG_SKIP_LOADER=false
FLAG_SKIP_GUI=false

# ── Helpers ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
skip()  { echo -e "  ${YELLOW}SKIP${NC} $*"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
step()  { echo ""; echo -e " ${CYAN}[$1/$TOTAL]${NC} $2"; }
done_s(){ echo -e "  ${GREEN}\u2713${NC} $*"; }

log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

step_completed() {
    mkdir -p "$STATE_DIR"
    echo "$1" >> "$STEP_FILE" 2>/dev/null || true
}
step_is_completed() {
    [ -f "$STEP_FILE" ] && grep -qxF "$1" "$STEP_FILE" 2>/dev/null
}

cleanup() {
    rm -f "$LOCK_FILE"
}
trap cleanup EXIT

# ── Parse args ──────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --resume)      FLAG_RESUME=true ;;
        --skip-schedulers) FLAG_SKIP_SCHED=true ;;
        --skip-loader)    FLAG_SKIP_LOADER=true ;;
        --skip-gui)       FLAG_SKIP_GUI=true ;;
        --help|-h)
            echo "Usage: $0 [--resume] [--skip-schedulers] [--skip-loader] [--skip-gui]"
            exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

# ── Preflight ───────────────────────────────────────────────────────────
echo ""
echo -e "  ${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "  ${CYAN}║       SCX Switcher — Installer       ║${NC}"
echo -e "  ${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root. The script uses sudo when needed."
fi

if [ -f "$LOCK_FILE" ]; then
    fail "Another install is in progress (lock: $LOCK_FILE). Remove it if stuck."
fi
echo "1" > "$LOCK_FILE"

exec > >(tee -a "$LOG") 2>&1
log "=== scx-switcher install started ==="

TOTAL=8

# ── Step 1: Kernel / Distro compatibility check ─────────────────────────
step 1 "System compatibility check"
if step_is_completed "kernel" && $FLAG_RESUME; then
    skip "already verified"
else
    IS_TRIXIE=false
    if grep -q "trixie" /etc/os-release 2>/dev/null || \
       grep -q "^13" /etc/debian_version 2>/dev/null; then
        IS_TRIXIE=true
    fi

    if [ ! -e /sys/kernel/sched_ext/state ]; then
        if $IS_TRIXIE; then
            KERNEL=$(uname -r)
            info "Kernel $KERNEL does not support sched_ext, but Debian Trixie detected."
            info "Installation will proceed. Reboot into a 6.12+ kernel to use sched_ext."
        else
            KERNEL=$(uname -r)
            fail "Kernel $KERNEL does not support sched_ext (need >= 6.12 with CONFIG_SCHED_CLASS_EXT=y).\n  Install a backports kernel first."
        fi
    else
        KERNEL=$(uname -r)
        info "Kernel $KERNEL supports sched_ext"
    fi
    step_completed "kernel"
    ok
fi

# ── Step 2: Install build dependencies ──────────────────────────────────
step 2 "Installing build dependencies"
if step_is_completed "deps" && $FLAG_RESUME; then
    skip "already installed"
else
    info "Updating apt package index..."
    sudo apt-get update || fail "apt update failed"

    KERNEL_VER=$(uname -r)
    # Try exact kernel headers first, fall back to generic
    HEADER_PKG="linux-headers-${KERNEL_VER}"
    if ! apt-cache show "$HEADER_PKG" &>/dev/null; then
        info "Exact kernel headers package '$HEADER_PKG' not found, using linux-headers-amd64"
        HEADER_PKG="linux-headers-amd64"
    fi

    info "Installing packages (including $HEADER_PKG)..."
    sudo apt-get install -y \
        git curl clang llvm libelf-dev libbpf-dev libzstd-dev \
        zlib1g-dev libseccomp-dev pkg-config build-essential bpftool \
        "$HEADER_PKG" qt6-base-dev libgl-dev polkitd \
        || fail "apt install failed"

    step_completed "deps"
    ok
fi

# ── Step 3: Install Rust (sandboxed) ────────────────────────────────────
step 3 "Rust toolchain (sandboxed)"
if [ -f "$CARGO_HOME/bin/cargo" ]; then
    skip "already available at $CARGO_HOME"
else
    info "Installing Rust into sandbox ($CARGO_HOME)..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
        | sh -s -- -y --no-modify-path || fail "rustup installation failed"
    . "$CARGO_HOME/env"
    rustup default stable || fail "rustup default failed"
    step_completed "rust"
    ok
fi

export PATH="$CARGO_HOME/bin:$PATH"

export CC=clang CXX=clang++

# ── Step 4: Build & install scx schedulers ──────────────────────────────
if ! $FLAG_SKIP_SCHED; then
    step 4 "Building sched-ext schedulers (scx)"
    if step_is_completed "scx" && $FLAG_RESUME; then
        skip "already built"
    else
        info "Cloning scx repository..."
        rm -rf "$BUILD/scx"
        git clone --depth 1 "$SCX_REPO" "$BUILD/scx" || fail "clone failed"

        info "Building scx (this may take a while)..."
        cargo build --release -j "$(nproc)" --manifest-path "$BUILD/scx/Cargo.toml" \
            || fail "scx build failed"

        info "Installing scx binaries..."
        for f in "$BUILD/scx/target/release/scx_"*; do
            [ -x "$f" ] || continue
            sudo install -Dm755 "$f" /usr/bin/ || fail "install of $(basename "$f") failed"
        done

        step_completed "scx"
        local scx_hash
        scx_hash=$(git -C "$BUILD/scx" rev-parse HEAD 2>/dev/null || true)
        if [ -n "$scx_hash" ]; then
            sudo mkdir -p /var/lib/scx-switcher
            echo "SCX_COMMIT=$scx_hash" | sudo tee /var/lib/scx-switcher/versions >/dev/null
        fi
        ok
    fi
else
    info "Step 4 skipped (--skip-schedulers)"
fi

# ── Step 5: Build & install scx-loader ─────────────────────────────────
if ! $FLAG_SKIP_LOADER; then
    step 5 "Building scx-loader (scxctl + scx_loader)"
    if step_is_completed "loader" && $FLAG_RESUME; then
        skip "already built"
    else
        info "Cloning scx-loader repository..."
        rm -rf "$BUILD/loader"
        git clone --depth 1 "$LOADER_REPO" "$BUILD/loader" || fail "clone failed"

        info "Building scx-loader..."
        cargo build --release -j "$(nproc)" --manifest-path "$BUILD/loader/Cargo.toml" \
            || fail "scx-loader build failed"

        info "Installing scx-loader binaries..."
        sudo install -Dm755 "$BUILD/loader/target/release/scx_loader" /usr/bin/scx_loader
        sudo install -Dm755 "$BUILD/loader/target/release/scxctl" /usr/bin/scxctl

        info "Installing system files..."
        sudo install -Dm644 "$BUILD/loader/services/scx_loader.service" \
            /usr/lib/systemd/system/scx_loader.service
        sudo install -Dm644 "$BUILD/loader/services/org.scx.Loader.service" \
            /usr/share/dbus-1/system-services/org.scx.Loader.service
        sudo install -Dm644 "$BUILD/loader/configs/org.scx.Loader.conf" \
            /usr/share/dbus-1/system.d/org.scx.Loader.conf
        sudo install -Dm644 "$BUILD/loader/configs/org.scx.Loader.xml" \
            /usr/share/dbus-1/interfaces/org.scx.Loader.xml
        sudo install -Dm644 "$BUILD/loader/configs/org.scx.Loader.policy" \
            /usr/share/polkit-1/actions/org.scx.Loader.policy

        info "Writing default config..."
        sudo mkdir -p /etc/scx_loader
        printf 'default_sched = "scx_bpfland"\ndefault_mode = "Auto"\n' | \
            sudo tee /etc/scx_loader/config.toml >/dev/null || fail "config write failed"

        step_completed "loader"
        local loader_hash
        loader_hash=$(git -C "$BUILD/loader" rev-parse HEAD 2>/dev/null || true)
        if [ -n "$loader_hash" ]; then
            echo "SCX_LOADER_COMMIT=$loader_hash" | sudo tee -a /var/lib/scx-switcher/versions >/dev/null
        fi
        ok
    fi
else
    info "Step 5 skipped (--skip-loader)"
fi

# ── Step 6: Build & install GUI ────────────────────────────────────────
if ! $FLAG_SKIP_GUI; then
    step 6 "Building SCX Switcher GUI"
    if step_is_completed "gui" && $FLAG_RESUME; then
        skip "already built"
    else
        info "Configuring cmake..."
        cd "$SCRIPT_DIR"
        rm -rf build
        cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
            || fail "cmake configure failed"

        info "Building..."
        cmake --build build -j "$(nproc)" || fail "cmake build failed"

        info "Installing GUI + PolKit policy..."
        sudo cmake --install build || fail "cmake install failed"

        step_completed "gui"
        ok
    fi
else
    info "Step 6 skipped (--skip-gui)"
fi

# ── Step 7: Install systemd drop-in for kernel check ────────────────────
if ! $FLAG_SKIP_LOADER; then
    step 7 "Installing systemd kernel-compatibility drop-in"
    if step_is_completed "dropin" && $FLAG_RESUME; then
        skip "already installed"
    else
        info "Installing systemd drop-in to skip scx_loader when kernel lacks sched_ext..."
        sudo mkdir -p /etc/systemd/system/scx_loader.service.d
        sudo cp "$SCRIPT_DIR/data/scx_loader-kernel-check.conf" \
            /etc/systemd/system/scx_loader.service.d/kernel-check.conf
        sudo systemctl daemon-reload 2>/dev/null || true
        step_completed "dropin"
        ok
    fi
else
    info "Step 7 skipped (--skip-loader)"
fi

# ── Step 8: Cleanup ─────────────────────────────────────────────────────
step 8 "Cleanup"
rm -rf "$BUILD" "$STATE_DIR" "$LOCK_FILE"
done_s "Installation complete."
echo ""
info "To start using:"
info "  1. Reboot or run: sudo systemctl start scx_loader.service"
info "  2. Launch: scx-switcher"
echo ""
info "Log saved to: $LOG"
echo ""

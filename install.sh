#!/usr/bin/env bash
# scx-switcher — Install the GUI + latest scx-scheds / scx-tools from scx-bundler
set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────
REPO="https://github.com/hmwassim/scx-bundler"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo "amd64")}"

# Temp workspace (mktemp avoids predictable /tmp races on multi-user systems)
TMP_DIR=$(mktemp -d /tmp/scx-switcher.XXXXXX)
LOG="$TMP_DIR/install.log"
STATE_DIR="$TMP_DIR/state"
DEB_DIR="$TMP_DIR/debs"
STEP_FILE="$STATE_DIR/completed_steps"
LOCK_DIR="/tmp/scx-switcher-install.lock"

FLAG_RESUME=false
FLAG_SKIP_GUI=false
FLAG_LOCAL_DEBS=""
FLAG_DRY_RUN=false

# ── Helpers ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
skip()  { echo -e "  ${YELLOW}SKIP${NC} $*"; }
warn()  { echo -e "  ${YELLOW}WARN${NC} $*"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }
step()  { echo ""; echo -e " ${CYAN}[$1/$TOTAL]${NC} $2"; }
log()   { echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

step_completed() {
    mkdir -p "$STATE_DIR"
    echo "$1" >> "$STEP_FILE" 2>/dev/null || true
}
step_is_completed() {
    [ -f "$STEP_FILE" ] && grep -qxF "$1" "$STEP_FILE" 2>/dev/null
}

verify_sha256() {
    (cd "$1" && grep "$2" SHA256SUMS | sha256sum -c -) >/dev/null 2>&1 || return 1
}

cleanup() {
    [ -n "${TMP_DIR:-}" ] && rm -rf "$TMP_DIR"
    rmdir "${LOCK_DIR:-/tmp/scx-switcher-install.lock}" 2>/dev/null || true
}
trap cleanup EXIT

# ── Parse args ──────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --resume)       FLAG_RESUME=true ;;
        --skip-gui)     FLAG_SKIP_GUI=true ;;
        --from-deb=*)   FLAG_LOCAL_DEBS="${arg#*=}" ;;
        --dry-run)      FLAG_DRY_RUN=true ;;
        --repo=*)       REPO="${arg#*=}" ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --resume           Skip previously completed steps"
            echo "  --skip-gui         Skip building the GUI"
            echo "  --from-deb=DIR     Install .debs from local path instead of downloading"
            echo "  --dry-run          Show what would be installed without making changes"
            echo "  --repo=URL         GitHub repo for releases (default: $REPO)"
            exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

if $FLAG_DRY_RUN; then DRY_PREFIX="[DRY-RUN] "; else DRY_PREFIX=""; fi

# ── Preflight ───────────────────────────────────────────────────────────
echo ""
echo -e "  ${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "  ${CYAN}║       SCX Switcher — Installer       ║${NC}"
echo -e "  ${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root. The script uses sudo when needed."
fi

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    fail "Another install is in progress (lock: $LOCK_DIR). Remove it if stuck."
fi

exec > >(tee -a "$LOG") 2>&1 || true
log "=== scx-switcher install started ==="

TOTAL=6

# ── Step 1: Kernel / Distro compatibility check ─────────────────────────
step 1 "System compatibility check"
if step_is_completed "kernel" && $FLAG_RESUME; then
    skip "already verified"
else
    info "${DRY_PREFIX}Checking kernel compatibility..."
    IS_TRIXIE=false
    if grep -qi "trixie" /etc/os-release 2>/dev/null || \
       grep -q "^13$" /etc/debian_version 2>/dev/null || \
       grep -q "^13\." /etc/debian_version 2>/dev/null; then
        IS_TRIXIE=true
    fi

    if [ ! -e /sys/kernel/sched_ext/state ]; then
        KERNEL=$(uname -r)
        if $IS_TRIXIE; then
            info "Kernel $KERNEL does not support sched_ext, but Debian Trixie detected."
            info "Installation will proceed. Reboot into a 6.12+ kernel to use sched_ext."
        else
            info "Checking for backports or testing kernels..."
            if dpkg -l | grep -q "linux-image.*-backports\|linux-image.*-testing" 2>/dev/null; then
                KERNEL=$(uname -r)
                info "Using backports/testing kernel: $KERNEL"
            else
                fail "Kernel $KERNEL does not support sched_ext (need >= 6.12 with CONFIG_SCHED_CLASS_EXT=y).\n  Install a backports or testing kernel first."
            fi
        fi
    else
        KERNEL=$(uname -r)
        info "Kernel $KERNEL supports sched_ext"
    fi
    step_completed "kernel"
    ok
fi

# ── Step 2: Install dependencies ────────────────────────────────────────
step 2 "Installing dependencies"
if step_is_completed "deps" && $FLAG_RESUME; then
    skip "already installed"
else
    info "${DRY_PREFIX}Installing build + runtime packages..."
    if ! $FLAG_DRY_RUN; then
        sudo apt-get update || fail "apt update failed"
        sudo apt-get install -y \
            qt6-base-dev libgl-dev cmake g++ polkitd pkexec curl jq \
            || fail "apt install failed"
    fi
    step_completed "deps"
    ok
fi

# ── Step 3: Install scx-scheds + scx-tools ──────────────────────────────
step 3 "Installing scx-scheds + scx-tools"
if command -v scxctl &>/dev/null && command -v scx_loader &>/dev/null; then
    skip "already installed (scxctl and scx_loader found)"
else
    if step_is_completed "debs" && $FLAG_RESUME; then
        skip "already installed"
    else
        rm -rf "$DEB_DIR"
        mkdir -p "$DEB_DIR"

        if ! $FLAG_DRY_RUN; then
            if [ -n "$FLAG_LOCAL_DEBS" ]; then
                info "Using .debs from $FLAG_LOCAL_DEBS..."
                warn "Skipping integrity verification for local .deb files"
                cp "$FLAG_LOCAL_DEBS"/scx-scheds_*_"${ARCH}".deb "$DEB_DIR/" 2>/dev/null || fail "scx-scheds .deb not found in $FLAG_LOCAL_DEBS"
                cp "$FLAG_LOCAL_DEBS"/scx-tools_*_"${ARCH}".deb  "$DEB_DIR/" 2>/dev/null || fail "scx-tools .deb not found in $FLAG_LOCAL_DEBS"
            else
                # Resolve latest tag via HTTP redirect — no API calls (no rate limit)
                LATEST_URL=$(curl -sIL -o /dev/null -w '%{url_effective}' "https://github.com/${REPO#https://github.com/}/releases/latest" 2>/dev/null || true)
                TAG=$(basename "$LATEST_URL" 2>/dev/null || true)
                if [ -z "$TAG" ] || [ "$TAG" = "latest" ]; then
                    fail "Could not determine latest release from $REPO"
                fi
                VERSION="${TAG#v}"
                BASE_URL="$REPO/releases/download/$TAG"
                info "Latest release: $TAG"
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
                curl -sL "$BASE_URL/$DEB_SCHEDS" -o "$DEB_DIR/$DEB_SCHEDS" || fail "scx-scheds download failed"
                if [ "$HAS_SUMS" = "1" ] && ! verify_sha256 "$DEB_DIR" "$DEB_SCHEDS"; then
                    fail "scx-scheds SHA256 mismatch"
                fi
                ok
                info "Downloading scx-tools..."
                DEB_TOOLS="scx-tools_${VERSION}_${ARCH}.deb"
                curl -sL "$BASE_URL/$DEB_TOOLS"  -o "$DEB_DIR/$DEB_TOOLS"  || fail "scx-tools download failed"
                if [ "$HAS_SUMS" = "1" ] && ! verify_sha256 "$DEB_DIR" "$DEB_TOOLS"; then
                    fail "scx-tools SHA256 mismatch"
                fi
                ok
            fi

            info "Installing scx-scheds + scx-tools..."
            deb_files=("$DEB_DIR"/*.deb)
            if [ ${#deb_files[@]} -eq 0 ] || [ ! -f "${deb_files[0]}" ]; then
                fail "No .deb files found in $DEB_DIR"
            fi
            sudo apt install -y "${deb_files[@]}" || fail "install failed"
        else
            info "[DRY-RUN] Would download and install scx-scheds + scx-tools from latest release"
        fi
        step_completed "debs"
        ok
    fi
fi

# ── Step 4: Build & install GUI ─────────────────────────────────────────
if ! $FLAG_SKIP_GUI; then
    step 4 "Building SCX Switcher GUI"
    if step_is_completed "gui" && $FLAG_RESUME; then
        skip "already built"
    else
        if ! $FLAG_DRY_RUN; then
            info "Configuring cmake..."
            cd "$SCRIPT_DIR"
            rm -rf build
            cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
                || fail "cmake configure failed"

            info "Building..."
            cmake --build build -j "$(nproc)" || fail "cmake build failed"

            info "Installing GUI + PolKit policy..."
            sudo cmake --install build || fail "cmake install failed"
        else
            info "[DRY-RUN] Would configure, build, and install GUI"
        fi
        step_completed "gui"
        ok
    fi
else
    info "Step 4 skipped (--skip-gui)"
fi

# ── Step 5: Install systemd drop-in for kernel check ────────────────────
step 5 "Installing systemd kernel-compatibility drop-in"
if step_is_completed "dropin" && $FLAG_RESUME; then
    skip "already installed"
else
    info "${DRY_PREFIX}Installing systemd kernel-check drop-in..."
    if ! $FLAG_DRY_RUN; then
        sudo mkdir -p /etc/systemd/system/scx_loader.service.d
        sudo cp "$SCRIPT_DIR/data/scx_loader-kernel-check.conf" \
            /etc/systemd/system/scx_loader.service.d/kernel-check.conf
        sudo systemctl daemon-reload 2>/dev/null || true
    fi
    step_completed "dropin"
    ok
fi

# ── Step 6: Cleanup ─────────────────────────────────────────────────────
step 6 "Cleanup"
if ! $FLAG_DRY_RUN; then
    echo ""
    info "scx-scheds, scx-tools, and scx-switcher GUI installed."
    info "Launch: scx-switcher"
else
    echo ""
    info "[DRY-RUN] Installation complete (no changes made)."
fi
echo ""
info "Log saved to: $LOG"
echo ""

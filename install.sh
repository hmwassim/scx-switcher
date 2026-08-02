#!/usr/bin/env bash
# scx-switcher — Universal installer
# Installs the GUI plus scx-scheds / scx-tools from native packages, falling
# back to a cargo build where no packages exist. Works across Debian/Ubuntu
# (apt), Fedora/RHEL (dnf), openSUSE (zypper), Arch (pacman), Solus (eopkg)
# and Alpine (apk).
set -euo pipefail

# ── Config ──────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OS_RELEASE="${OS_RELEASE:-/etc/os-release}"
SCX_GIT_URL="https://github.com/sched-ext/scx"
SCX_LOADER_GIT_URL="https://github.com/sched-ext/scx-loader"
SCX_TAG="v1.1.2"

# Temp workspace (mktemp avoids predictable /tmp races on multi-user systems)
TMP_DIR=$(mktemp -d /tmp/scx-switcher.XXXXXX)
LOG="$TMP_DIR/install.log"
STATE_DIR="$TMP_DIR/state"
STEP_FILE="$STATE_DIR/completed_steps"
LOCK_DIR="/tmp/scx-switcher-install.lock"

FLAG_RESUME=false
FLAG_SKIP_GUI=false
FLAG_LOCAL_PKGS=""
FLAG_DRY_RUN=false
FLAG_OS_RELEASE=""
FLAG_SKIP_DEPS=false

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

cleanup() {
    [ -n "${TMP_DIR:-}" ] && rm -rf "$TMP_DIR"
    rmdir "${LOCK_DIR:-/tmp/scx-switcher-install.lock}" 2>/dev/null || true
}
trap cleanup EXIT

# ── Parse args ──────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --resume)         FLAG_RESUME=true ;;
        --skip-gui)       FLAG_SKIP_GUI=true ;;
        --skip-deps)      FLAG_SKIP_DEPS=true ;;
        --from-local=*)   FLAG_LOCAL_PKGS="${arg#*=}" ;;
        --dry-run)        FLAG_DRY_RUN=true ;;
        --os-release=*)   FLAG_OS_RELEASE="${arg#*=}" ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --resume           Skip previously completed steps"
            echo "  --skip-gui         Skip building the GUI"
            echo "  --skip-deps        Skip installing build dependencies"
            echo "  --from-local=DIR   Install scx packages from local package files"
            echo "  --dry-run          Show what would be installed without making changes"
            echo "  --os-release=FILE  Use an alternative os-release file (testing)"
            exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

if $FLAG_DRY_RUN; then DRY_PREFIX="[DRY-RUN] "; else DRY_PREFIX=""; fi

# ── Distro detection ────────────────────────────────────────────────────
osval() {
    awk -F= -v k="$1" '$1==k { gsub(/^"|"$/, "", $2); print $2; exit }' "$OS_RELEASE_FILE"
}

detect_os() {
    OS_RELEASE_FILE="${FLAG_OS_RELEASE:-$OS_RELEASE}"
    if [ ! -f "$OS_RELEASE_FILE" ]; then
        fail "os-release not found: $OS_RELEASE_FILE"
    fi
    DISTRO_ID="$(osval ID)"
    DISTRO_ID_LIKE="$(osval ID_LIKE)"
    DISTRO_PRETTY="$(osval PRETTY_NAME)"
}

detect_pkgmgr() {
    local hay="$DISTRO_ID $DISTRO_ID_LIKE"
    case "$hay" in
        *debian*|*ubuntu*)        PM=apt ;;
        *suse*)                   PM=zypper ;;
        *fedora*|*rhel*|*centos*|*rocky*|*alma*) PM=dnf ;;
        *arch*|*manjaro*|*cachyos*|*endeavouros*|*garuda*) PM=pacman ;;
        *solus*)                  PM=eopkg ;;
        *alpine*)                 PM=apk ;;
        *)
            # Unknown distro: probe for whichever package manager exists
            for p in apt-get dnf zypper pacman eopkg apk; do
                if command -v "$p" >/dev/null 2>&1; then
                    PM="$p"; [ "$PM" = "apt-get" ] && PM=apt
                    break
                fi
            done
            [ -n "${PM:-}" ] || fail "Unsupported distribution ($DISTRO_PRETTY). Install cmake, Qt6, scx-scheds and scx-tools manually, then build with ./build.sh"
            ;;
    esac
}

detect_os
detect_pkgmgr

# ── Package-manager helpers ─────────────────────────────────────────────
build_deps() {
    case "$PM" in
        apt)     echo "qt6-base-dev libgl-dev cmake g++ polkitd pkexec curl" ;;
        dnf)     echo "qt6-qtbase-devel cmake gcc-c++ polkit dnf-plugins-core curl" ;;
        zypper)  echo "qt6-base-common-devel qt6-core-devel qt6-widgets-devel cmake gcc-c++ polkit curl" ;;
        pacman)  echo "qt6-base cmake gcc polkit curl" ;;
        eopkg)   echo "qt6-base-devel cmake g++ polkit curl" ;;
        apk)     echo "qt6-qtbase-dev cmake g++ polkit curl" ;;
    esac
}

cargo_deps() {
    # Rust toolchain itself comes from rustup (see ensure_rust) — only the
    # C/BPF build dependencies are installed from the package manager.
    # pkg-config: apt ships the real package; dnf/zypper/pacman/eopkg use
    # pkgconf (which provides the pkg-config binary), apk uses pkgconf too.
    case "$PM" in
        apt)     echo "clang llvm pkg-config libelf-dev libbpf-dev libseccomp-dev protobuf-compiler" ;;
        dnf)     echo "clang llvm pkgconf elfutils-libelf-devel libbpf-devel libseccomp-devel protobuf-compiler" ;;
        zypper)  echo "clang llvm pkgconf libelf-devel libbpf-devel libseccomp-devel protobuf-devel" ;;
        pacman)  echo "clang llvm pkgconf elfutils libbpf libseccomp protobuf" ;;
        eopkg)   echo "clang llvm pkgconf libelf-devel libbpf-devel libseccomp-devel protobuf-devel" ;;
        apk)     echo "clang llvm pkgconf elfutils-dev libbpf-dev libseccomp-dev protobuf-dev" ;;
    esac
}

scx_pkgs() {
    # zypper's "scx" ships only the schedulers (no scxctl/scx_loader) — see
    # install_scx_loader_from_source. Fedora's scx-scheds lives in a COPR,
    # enabled by pm_prepare_scx. Debian/Ubuntu have no native scx packages
    # (they come from the user's scx-bundler repo); the failing apt install
    # below falls back to a source build.
    case "$PM" in
        apt|dnf|pacman|eopkg) echo "scx-scheds scx-tools" ;;
        zypper)               echo "scx" ;;
        apk)                  echo "" ;;
    esac
}

pm_prepare_scx() {
    # Run before installing native scx packages: enable the CachyOS COPR that
    # ships scx-scheds/scx-tools for Fedora (not present in base repos).
    case "$PM" in
        dnf)
            info "Enabling CachyOS COPR for Fedora scx packages..."
            if ! $FLAG_DRY_RUN; then
                if dnf copr --help >/dev/null 2>&1; then
                    sudo dnf copr enable -y bieszczaders/kernel-cachyos-addons
                else
                    warn "dnf copr plugin not available; adding COPR repo manually"
                    sudo dnf config-manager --add-repo \
                        "https://copr.fedorainfracloud.org/coprs/bieszczaders/kernel-cachyos-addons/repo/fedora-\$(rpm -E %fedora)/bieszczaders-kernel-cachyos-addons-fedora-\$(rpm -E %fedora).repo"
                fi
            fi
            ;;
    esac
}

pm_install() {
    case "$PM" in
        apt)    sudo apt-get update -qq && sudo apt-get install -y "$@" ;;
        dnf)    sudo dnf install -y "$@" ;;
        zypper) sudo zypper install -y "$@" ;;
        pacman) sudo pacman -S --noconfirm "$@" ;;
        eopkg)  sudo eopkg install --yes "$@" ;;
        apk)    sudo apk add "$@" ;;
    esac
}

ensure_rust() {
    if command -v rustup >/dev/null 2>&1; then
        if ! command -v cargo >/dev/null 2>&1 && [ -f "$HOME/.cargo/env" ]; then
            . "$HOME/.cargo/env" || true
        fi
        return
    fi
    if [ -x "$HOME/.cargo/bin/cargo" ]; then
        PATH="$HOME/.cargo/bin:$PATH"
        return
    fi
    # Package-manager Rust (esp. Debian) is often too old for sched-ext, so
    # always install a recent toolchain from the mainstream rustup installer.
    info "Installing Rust via rustup (recent mainstream toolchain)..."
    if ! $FLAG_DRY_RUN; then
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
        . "$HOME/.cargo/env" || true
    fi
}

install_loader_bins() {
    local src="$1"
    info "Installing scx_loader + scxctl system files..."
    sudo install -Dm755 "$src"/scx-loader/target/release/scx_loader /usr/bin/scx_loader
    sudo install -Dm755 "$src"/scx-loader/target/release/scxctl /usr/bin/scxctl
    sudo install -Dm644 "$src"/scx-loader/services/scx_loader.service /usr/lib/systemd/system/ 2>/dev/null || true
    sudo install -Dm644 "$src"/scx-loader/services/org.scx.Loader.service /usr/share/dbus-1/system-services/ 2>/dev/null || true
    sudo install -Dm644 "$src"/scx-loader/configs/org.scx.Loader.conf /usr/share/dbus-1/system.d/ 2>/dev/null || true
    sudo install -Dm644 "$src"/scx-loader/configs/org.scx.Loader.xml /usr/share/dbus-1/interfaces/ 2>/dev/null || true
    sudo install -Dm644 "$src"/scx-loader/configs/org.scx.Loader.policy /usr/share/polkit-1/actions/ 2>/dev/null || true
    if command -v systemctl >/dev/null 2>&1; then
        sudo systemctl daemon-reload 2>/dev/null || true
    fi
}

install_scx_loader_from_source() {
    info "Building scx_loader + scxctl from source (no native scx-tools)..."
    if ! $FLAG_DRY_RUN; then
        pm_install $(cargo_deps) || fail "Failed to install cargo build dependencies"
        ensure_rust
        command -v cargo >/dev/null 2>&1 || fail "cargo still unavailable after rustup install"

        local src="$TMP_DIR/src"
        mkdir -p "$src"
        info "Fetching scx-loader ${SCX_TAG}..."
        git clone -q --depth 1 --branch "$SCX_TAG" "$SCX_LOADER_GIT_URL" "$src/scx-loader" || fail "scx-loader clone failed"
        info "Building scx_loader + scxctl..."
        (cd "$src/scx-loader" && cargo build --release) || fail "cargo build (scx-loader) failed"
        install_loader_bins "$src"
    else
        info "[DRY-RUN] Would cargo-build scx_loader + scxctl from source"
    fi
}

install_scx_from_source() {
    info "Building scx-scheds + scx-tools from source (no native packages)..."
    if ! $FLAG_DRY_RUN; then
        pm_install $(cargo_deps) || fail "Failed to install cargo build dependencies"
        ensure_rust
        command -v cargo >/dev/null 2>&1 || fail "cargo still unavailable after rustup install"

        local src="$TMP_DIR/src"
        mkdir -p "$src"

        info "Fetching scx ${SCX_TAG}..."
        git clone -q --depth 1 --branch "$SCX_TAG" "$SCX_GIT_URL" "$src/scx" || fail "scx clone failed"
        info "Building schedulers (this can take several minutes)..."
        (cd "$src/scx" && cargo build --release) || fail "cargo build (scx) failed"
        for f in "$src"/scx/target/release/scx_*; do
            [ -x "$f" ] && sudo install -Dm755 "$f" /usr/bin/
        done

        install_scx_loader_from_source
    else
        info "[DRY-RUN] Would cargo-build scx-scheds + scx-tools from source"
    fi
}

# ── Preflight ───────────────────────────────────────────────────────────
echo ""
echo -e "  ${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "  ${CYAN}║       SCX Switcher — Installer       ║${NC}"
echo -e "  ${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root. The script uses sudo when needed."
fi

if ! command -v sudo >/dev/null 2>&1; then
    fail "sudo not found. Install sudo (or run the install steps manually)."
fi

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    fail "Another install is in progress (lock: $LOCK_DIR). Remove it if stuck."
fi

exec > >(tee -a "$LOG") 2>&1 || true
log "=== scx-switcher install started ($DISTRO_PRETTY) ==="

TOTAL=6

# ── Step 1: Kernel / Distro compatibility check ─────────────────────────
step 1 "System compatibility check"
if step_is_completed "kernel" && $FLAG_RESUME; then
    skip "already verified"
else
    info "${DRY_PREFIX}Distribution: ${DISTRO_PRETTY:-unknown}"
    info "${DRY_PREFIX}Package manager: $PM"
    if [ ! -e /sys/kernel/sched_ext/state ]; then
        KERNEL=$(uname -r)
        warn "Kernel $KERNEL does not expose /sys/kernel/sched_ext."
        warn "A Linux 6.12+ kernel with sched-ext is required. Installation continues; reboot into a supported kernel before using schedulers."
    else
        KERNEL=$(uname -r)
        info "Kernel $KERNEL supports sched_ext"
    fi
    step_completed "kernel"
    ok
fi

# ── Step 2: Install dependencies ────────────────────────────────────────
step 2 "Installing dependencies"
if $FLAG_SKIP_DEPS; then
    skip "skipped (--skip-deps)"
elif step_is_completed "deps" && $FLAG_RESUME; then
    skip "already installed"
else
    info "${DRY_PREFIX}Installing build + runtime packages..."
    if ! $FLAG_DRY_RUN; then
        pm_install $(build_deps) || fail "Failed to install dependencies via $PM"
    fi
    step_completed "deps"
    ok
fi

# ── Step 3: Install scx-scheds + scx-tools ──────────────────────────────
step 3 "Installing scx-scheds + scx-tools"
if command -v scxctl &>/dev/null && command -v scx_loader &>/dev/null; then
    skip "already installed (scxctl and scx_loader found)"
else
    if step_is_completed "scx" && $FLAG_RESUME; then
        skip "already installed"
    else
        if [ -n "$FLAG_LOCAL_PKGS" ]; then
            info "${DRY_PREFIX}Installing scx packages from $FLAG_LOCAL_PKGS..."
            if ! $FLAG_DRY_RUN; then
                mapfile -t local_files < <(find "$FLAG_LOCAL_PKGS" -maxdepth 1 -type f ! -name 'SHA256SUMS' | sort)
                [ ${#local_files[@]} -gt 0 ] || fail "No package files found in $FLAG_LOCAL_PKGS"
                pm_install "${local_files[@]}" || fail "Failed to install local packages via $PM"
            fi
        else
            native_pkgs="$(scx_pkgs)"
            if [ -n "$native_pkgs" ]; then
                info "${DRY_PREFIX}Installing $native_pkgs via $PM..."
                pm_prepare_scx
                if [ "$PM" = "zypper" ]; then
                    warn "openSUSE's scx package ships schedulers only — scx_loader + scxctl will be built from source"
                fi
                if ! $FLAG_DRY_RUN; then
                    if pm_install $native_pkgs; then
                        if [ "$PM" = "zypper" ]; then
                            install_scx_loader_from_source
                        fi
                    else
                        warn "Native package install failed — falling back to source build"
                        install_scx_from_source
                    fi
                fi
            else
                warn "No native scx packages for $PM — building from source"
                install_scx_from_source
            fi
        fi
        step_completed "scx"
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

# ── Step 5: systemd kernel-compatibility drop-in ────────────────────────
step 5 "Installing systemd kernel-compatibility drop-in"
if [ ! -d /run/systemd/system ]; then
    skip "systemd not detected — drop-in not needed"
elif step_is_completed "dropin" && $FLAG_RESUME; then
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

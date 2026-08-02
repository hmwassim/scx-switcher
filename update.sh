#!/usr/bin/env bash
# scx-switcher — Universal updater
# Refreshes scx-scheds / scx-tools from native packages (or rebuilds from
# source when no packages exist) and rebuilds/updates the GUI.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OS_RELEASE="${OS_RELEASE:-/etc/os-release}"
SCX_GIT_URL="https://github.com/sched-ext/scx"
SCX_LOADER_GIT_URL="https://github.com/sched-ext/scx-loader"
SCX_TAG="v1.1.2"

TMP_DIR=$(mktemp -d /tmp/scx-switcher-update.XXXXXX)
LOG="$TMP_DIR/update.log"

FLAG_LOCAL_PKGS=""
FLAG_DRY_RUN=false
FLAG_OS_RELEASE=""

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
        --from-local=*) FLAG_LOCAL_PKGS="${arg#*=}" ;;
        --dry-run)      FLAG_DRY_RUN=true ;;
        --os-release=*) FLAG_OS_RELEASE="${arg#*=}" ;;
        --help|-h)
            echo "Usage: $0 [--from-local=DIR] [--dry-run] [--os-release=FILE]"
            exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

if $FLAG_DRY_RUN; then DRY_PREFIX="[DRY-RUN] "; else DRY_PREFIX=""; fi

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root. The script uses sudo when needed."
fi

if ! command -v sudo >/dev/null 2>&1; then
    fail "sudo not found. Install sudo (or run the update steps manually)."
fi

exec > >(tee -a "$LOG") 2>&1 || true
log "=== scx-switcher update started ==="

# ── Distro detection ────────────────────────────────────────────────────
osval() {
    awk -F= -v k="$1" '$1==k { gsub(/^"|"$/, "", $2); print $2; exit }' "$OS_RELEASE_FILE"
}

OS_RELEASE_FILE="${FLAG_OS_RELEASE:-$OS_RELEASE}"
[ -f "$OS_RELEASE_FILE" ] || fail "os-release not found: $OS_RELEASE_FILE"
DISTRO_ID="$(osval ID)"
DISTRO_ID_LIKE="$(osval ID_LIKE)"
DISTRO_PRETTY="$(osval PRETTY_NAME)"

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
            for p in apt-get dnf zypper pacman eopkg apk; do
                if command -v "$p" >/dev/null 2>&1; then
                    PM="$p"; [ "$PM" = "apt-get" ] && PM=apt
                    break
                fi
            done
            [ -n "${PM:-}" ] || fail "Unsupported distribution ($DISTRO_PRETTY). Update scx-scheds and scx-tools manually."
            ;;
    esac
}
detect_pkgmgr

scx_pkgs() {
    case "$PM" in
        apt|dnf|pacman|eopkg) echo "scx-scheds scx-tools" ;;
        zypper)               echo "scx" ;;
        apk)                  echo "" ;;
    esac
}

cargo_deps() {
    # Rust toolchain itself comes from rustup (see install.sh's ensure_rust)
    case "$PM" in
        apt)     echo "clang llvm pkg-config libelf-dev libbpf-dev libseccomp-dev protobuf-compiler" ;;
        dnf)     echo "clang llvm pkgconf elfutils-libelf-devel libbpf-devel libseccomp-devel protobuf-compiler" ;;
        zypper)  echo "clang llvm pkgconf libelf-devel libbpf-devel libseccomp-devel protobuf-devel" ;;
        pacman)  echo "clang llvm pkgconf elfutils libbpf libseccomp protobuf" ;;
        eopkg)   echo "clang llvm pkgconf libelf-devel libbpf-devel libseccomp-devel protobuf-devel" ;;
        apk)     echo "clang llvm pkgconf elfutils-dev libbpf-dev libseccomp-dev protobuf-dev" ;;
    esac
}

pm_install() {
    case "$PM" in
        apt)    sudo apt-get update -qq && sudo apt-get install -y --upgrade "$@" ;;
        dnf)    sudo dnf upgrade -y "$@" ;;
        zypper) sudo zypper update -y "$@" ;;
        pacman) sudo pacman -Syu --noconfirm "$@" ;;
        eopkg)  sudo eopkg upgrade --yes "$@" ;;
        apk)    sudo apk add --upgrade "$@" ;;
    esac
}

pm_prepare_scx() {
    # Ensure the CachyOS COPR (scx-scheds/scx-tools for Fedora) is present
    # before refreshing native scx packages.
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

echo ""
info "Updating SCX Switcher components ($DISTRO_PRETTY)..."
echo ""

# ── Step 1: Refresh scx-scheds + scx-tools ─────────────────────────────
if command -v scxctl &>/dev/null && command -v scx_loader &>/dev/null; then
    if [ -n "$FLAG_LOCAL_PKGS" ]; then
        info "${DRY_PREFIX}Updating scx packages from $FLAG_LOCAL_PKGS..."
        if ! $FLAG_DRY_RUN; then
            mapfile -t local_files < <(find "$FLAG_LOCAL_PKGS" -maxdepth 1 -type f ! -name 'SHA256SUMS' | sort)
            [ ${#local_files[@]} -gt 0 ] || fail "No package files found in $FLAG_LOCAL_PKGS"
            pm_install "${local_files[@]}" || fail "Failed to update local packages via $PM"
        fi
    elif [ -n "$(scx_pkgs)" ]; then
        info "${DRY_PREFIX}Updating $(scx_pkgs) via $PM..."
        pm_prepare_scx
        if [ "$PM" = "zypper" ]; then
            warn "openSUSE's scx package ships schedulers only — scx_loader + scxctl will be rebuilt from source"
        fi
        if ! $FLAG_DRY_RUN; then
            if pm_install $(scx_pkgs); then
                if [ "$PM" = "zypper" ]; then
                    install_scx_loader_from_source
                fi
            else
                warn "Native package update failed — run install.sh to rebuild from source"
            fi
        fi
    else
        warn "No native scx packages for $PM — run install.sh to rebuild from source"
    fi
else
    info "scxctl/scx_loader not found — run install.sh first"
fi
ok

# ── Step 2: Rebuild GUI ─────────────────────────────────────────────────
info "${DRY_PREFIX}Rebuilding SCX Switcher GUI..."
if ! $FLAG_DRY_RUN; then
    cd "$SCRIPT_DIR"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr || fail "cmake configure failed"
    cmake --build build -j "$(nproc)" || fail "cmake build failed"
    sudo cmake --install build || fail "cmake install failed"
fi
ok

# ── Step 3: Refresh systemd kernel-check drop-in ────────────────────────
if [ -d /run/systemd/system ]; then
    info "${DRY_PREFIX}Installing systemd kernel-compatibility drop-in..."
    if ! $FLAG_DRY_RUN; then
        sudo mkdir -p /etc/systemd/system/scx_loader.service.d
        sudo cp "$SCRIPT_DIR/data/scx_loader-kernel-check.conf" \
            /etc/systemd/system/scx_loader.service.d/kernel-check.conf
        sudo systemctl daemon-reload 2>/dev/null || true
    fi
    ok
else
    skip "systemd not detected — drop-in not needed"
fi

echo ""
info "Update complete."
info "Restart scx-switcher to apply GUI changes."
echo ""

log "=== update finished ==="

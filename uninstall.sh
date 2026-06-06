#!/usr/bin/env bash
# scx-switcher — Complete uninstall of sched-ext + GUI
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "  ${CYAN}::${NC} $*"; }
ok()    { echo -e "  ${GREEN}OK${NC}"; }
warn()  { echo -e "  ${YELLOW}WARN${NC} $*"; }
fail()  { echo -e "  ${RED}FAIL${NC} $*"; exit 1; }

if [ "$(id -u)" -eq 0 ]; then
    fail "Do not run as root. The script uses sudo when needed."
fi

echo ""
echo -e "  ${YELLOW}╔══════════════════════════════════════╗${NC}"
echo -e "  ${YELLOW}║    SCX Switcher — Uninstaller        ║${NC}"
echo -e "  ${YELLOW}╚══════════════════════════════════════╝${NC}"
echo ""

# Confirm (skip if SKIP_PROMPT is set)
if [ "${SKIP_PROMPT:-}" != "1" ]; then
    read -r -p "  Remove all sched-ext components? [y/N] " REPLY
    if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
        info "Cancelled."
        exit 0
    fi
fi

# Stop service
info "Stopping scx_loader service..."
sudo systemctl disable --now scx_loader 2>/dev/null || warn "scx_loader not running"
sudo systemctl disable --now scx_loader.service 2>/dev/null || true
ok

# Remove binaries
info "Removing binaries..."
sudo rm -f /usr/bin/scx_loader /usr/bin/scxctl /usr/bin/scx-switcher
for f in /usr/bin/scx_*; do
    [ -f "$f" ] || continue
    case "$(basename "$f")" in
        scx_arena_selftests|scx_beerland|scx_bpfland|scx_cake|scx_chaos|scx_cosmos|scx_flash|scx_flow|scx_lavd|scx_layered|scx_mitosis|scx_p2dq|scx_pandemonium|scx_rlfifo|scx_rustland|scx_rusty|scx_tickless)
            sudo rm -f "$f" ;;
    esac
done
ok

# Remove system files
info "Removing system files..."
sudo rm -f /usr/lib/systemd/system/scx_loader.service
sudo rm -rf /etc/systemd/system/scx_loader.service.d
sudo rm -f /usr/share/dbus-1/system-services/org.scx.Loader.service
sudo rm -f /usr/share/dbus-1/system.d/org.scx.Loader.conf
sudo rm -f /usr/share/dbus-1/interfaces/org.scx.Loader.xml
sudo rm -f /usr/share/polkit-1/actions/org.scx.Loader.policy
sudo rm -f /usr/share/polkit-1/actions/com.scx-switcher.policy
ok

# Remove config
info "Removing config..."
sudo rm -rf /etc/scx_loader
ok

# Remove desktop/metainfo/icons
info "Removing desktop files..."
sudo rm -f /usr/share/applications/scx-switcher.desktop
sudo rm -f /usr/share/metainfo/scx-switcher.metainfo.xml
sudo rm -f /usr/share/icons/hicolor/scalable/apps/scx-switcher.svg
ok

# Reload systemd
info "Reloading systemd..."
sudo systemctl daemon-reload
ok

# Clean user state
STATE_DIR="$HOME/.local/state/scx-switcher"
if [ -d "$STATE_DIR" ]; then
    info "Removing user state: $STATE_DIR"
    rm -rf "$STATE_DIR"
fi

# Clean version tracking state
VERSIONS_FILE="/var/lib/scx-switcher/versions"
if [ -f "$VERSIONS_FILE" ]; then
    info "Removing version state..."
    sudo rm -f "$VERSIONS_FILE"
    sudo rmdir "$(dirname "$VERSIONS_FILE")" 2>/dev/null || true
fi

echo ""
info "Uninstall complete."
info "You may also remove Rust: rustup self uninstall"
echo ""

#!/usr/bin/env bash
# scx-switcher — Uninstall the GUI only (leaves scx-scheds + scx-tools)
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
    read -r -p "  Remove scx-switcher GUI only? (scx-scheds + scx-tools stay) [y/N] " REPLY
    if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
        info "Cancelled."
        exit 0
    fi
fi

# Remove systemd kernel-check drop-in (installed by scx-switcher)
info "Removing systemd kernel-check drop-in..."
sudo rm -f /etc/systemd/system/scx_loader.service.d/kernel-check.conf
sudo rmdir /etc/systemd/system/scx_loader.service.d 2>/dev/null || true
sudo systemctl daemon-reload 2>/dev/null || true
ok

# Ask before disabling the service (may have been configured independently)
if systemctl is-active scx_loader.service &>/dev/null || systemctl is-enabled scx_loader.service &>/dev/null; then
    if [ "${SKIP_PROMPT:-}" = "1" ]; then
        REPLY="y"
    else
        read -r -p "  Disable and stop scx_loader.service (was enabled by scx-switcher)? [y/N] " REPLY
    fi
    if [[ "$REPLY" =~ ^[Yy]$ ]]; then
        info "Disabling scx_loader auto-start..."
        sudo systemctl disable --now scx_loader.service 2>/dev/null || true
        ok
    else
        info "Keeping scx_loader.service unchanged"
    fi
fi

# Remove GUI binary
info "Removing GUI binary..."
sudo rm -f /usr/bin/scx-switcher
ok

# Remove desktop/metainfo/icons
info "Removing desktop files..."
sudo rm -f /usr/share/applications/scx-switcher.desktop
sudo rm -f /usr/share/metainfo/scx-switcher.metainfo.xml
sudo rm -f /usr/share/icons/hicolor/scalable/apps/scx-switcher.svg
ok

# Remove PolKit policy
info "Removing PolKit policy..."
sudo rm -f /usr/share/polkit-1/actions/com.scx-switcher.policy
ok

# Remove kernel-check drop-in from share as well
sudo rm -f /usr/share/scx-switcher/scx_loader-kernel-check.conf
sudo rmdir /usr/share/scx-switcher 2>/dev/null || true

# Remove user state (ask first)
STATE_DIR="$HOME/.local/state/scx-switcher"
if [ -d "$STATE_DIR" ]; then
    if [ "${SKIP_PROMPT:-}" != "1" ]; then
        read -r -p "  Remove user state directory ($STATE_DIR)? [y/N] " REPLY
        if [[ "$REPLY" =~ ^[Yy]$ ]]; then
            info "Removing user state: $STATE_DIR"
            rm -rf "$STATE_DIR"
            ok
        else
            info "Keeping user state: $STATE_DIR"
        fi
    else
        info "Removing user state: $STATE_DIR"
        rm -rf "$STATE_DIR"
        ok
    fi
fi

echo ""
info "Uninstall complete."
echo ""
info "To also remove schedulers and tools:"
info "  sudo apt remove scx-scheds scx-tools"
echo ""

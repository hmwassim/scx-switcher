# DebForge SCX

Qt6 GUI for installing and managing sched-ext BPF CPU schedulers on **Debian 13+ (Trixie)**.

## Prerequisites

- Debian 13 (Trixie) or newer
- Kernel 6.12+ with sched-ext support (`/sys/kernel/sched_ext` exists)
- For backported kernels, matching `linux-headers-$(uname -r)` should be available

## Quick Start

```sh
# 1. Install everything (schedulers, loader, GUI)
./install.sh

# 2. Reboot (or: sudo systemctl start scx_loader.service)

# 3. Launch the GUI
debforge-scx
```

## Scripts

| Script | Purpose |
|--------|---------|
| `install.sh` | Full install: deps, Rust, scx schedulers, scx-loader/scxctl, GUI |
| `update.sh` | Rebuild & reinstall schedulers, loader, and GUI |
| `uninstall.sh` | Complete removal of all components |
| `build.sh` | Build just the GUI (requires existing scxctl) |

### install.sh options

```
--resume             Skip previously completed steps (safe to re-run after failure)
--skip-schedulers    Skip building scx schedulers
--skip-loader        Skip building scx-loader
--skip-gui           Skip building the GUI
```

## Building the .deb package

```sh
sudo apt install dpkg-dev debhelper cmake qt6-base-dev g++
dpkg-buildpackage -us -uc
```

## Unprivileged vs Privileged operations

- **scxctl list, scxctl get, systemctl is-enabled** run directly as your user
- **Stopping, starting, switching schedulers** use `pkexec` with PolKit `auth_admin_keep` caching (password once per session)
- The `com.debforge.scx.policy` action file in `/usr/share/polkit-1/actions/` enables credential caching

## Files installed

| Item | Location |
|------|----------|
| GUI binary | `/usr/bin/debforge-scx` |
| scxctl | `/usr/bin/scxctl` |
| scx_loader | `/usr/bin/scx_loader` |
| Schedulers | `/usr/bin/scx_*` |
| Config | `/etc/scx_loader/config.toml` |
| User state | `~/.local/state/debforge-scx/state.json` |
| PolKit action | `/usr/share/polkit-1/actions/com.debforge.scx.policy` |

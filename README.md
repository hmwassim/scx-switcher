# debforge-scx

A Qt6 GUI for installing and managing sched-ext (SCX) BPF CPU schedulers on Debian 13+ (Trixie).

## Prerequisites

- **Debian 13+ (Trixie)** — not tested on other distros
- **Kernel 6.12+** with sched-ext support (`/sys/kernel/sched_ext` must exist)

## Quick Start

```bash
git clone https://github.com/hmwassim/debforge-scx.git
cd debforge-scx
./install.sh
# Reboot, then:
debforge-scx
```

## Scripts

| Script | Description |
|--------|-------------|
| `install.sh` | Full unattended installation (kernel check, deps, Rust, schedulers, loader, GUI) |
| `update.sh`  | Incremental update (rebuild schedulers, loader, GUI) |
| `uninstall.sh` | Interactive removal of all installed components |
| `build.sh`   | Quick GUI-only build (for development) |

### install.sh options

```
--resume            Resume a previous interrupted install
--skip-schedulers   Skip building and installing scx schedulers
--skip-loader       Skip building scx-loader and scxctl
--skip-gui          Skip building the GUI
```

## Building a .deb package

```bash
dpkg-buildpackage -us -uc
```

## Architecture

- **debforge-scx** — Qt6 GUI binary (`/usr/bin/debforge-scx`)
- **scxctl** — CLI wrapper for scheduler control (`/usr/bin/scxctl`, installed by install.sh)
- **scx_loader** — systemd service for persistent scheduler at boot (`/usr/bin/scx_loader`, installed by install.sh)
- **scx schedulers** — BPF schedulers built from `scx/` (installed by install.sh)

Privileged operations (start/stop/switch schedulers, toggle service) use `pkexec` with PolKit caching (`auth_admin_keep`). Non-privileged operations (list, get status, systemctl read-only) run as the current user.

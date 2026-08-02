# SCX Switcher

Qt6 GUI for managing sched-ext BPF CPU schedulers. Distro-agnostic: detects
your distribution and works on Debian/Ubuntu (apt), Fedora/RHEL (dnf),
openSUSE (zypper), Arch (pacman), Solus (eopkg) and Alpine (apk).

## Requirements

- Linux kernel 6.12+ with sched-ext (`CONFIG_SCHED_CLASS_EXT`)
- [scx-scheds](https://github.com/sched-ext/scx) and
  [scx-tools](https://github.com/sched-ext/scx-loader) (provide `scxctl` and
  `scx_loader`)
- systemd (for the `scx_loader` service)
- pkexec or sudo (for privilege escalation)

## Install

```sh
./install.sh
```

The installer detects your package manager, installs `scx-scheds` +
`scx-tools` from native packages where available, falls back to a cargo build
from source otherwise, and builds/installs the GUI.

Individual steps:

```sh
# Build the GUI binary
./build.sh

# Install to /usr (needs root)
sudo cmake --install build

# Install only the scx packages for your distribution, e.g.:
# Debian/Ubuntu have no scx-scheds/scx-tools packages — use ./install.sh
# (source build) or install scx-bundler .debs. Other distros:
sudo dnf copr enable -y bieszczaders/kernel-cachyos-addons
sudo dnf install scx-scheds scx-tools        # Fedora (CachyOS COPR)
sudo pacman -S scx-scheds scx-tools          # Arch

# openSUSE: the "scx" package ships schedulers only — no scxctl/scx_loader.
# Install it and build the tools from source, or just run ./install.sh:
sudo zypper install scx
```

The installer enables the Fedora COPR automatically; on openSUSE it installs
the `scx` schedulers and builds `scx_loader` + `scxctl` from source, since the
native package does not ship them.

## Scripts

| Script | Purpose |
|--------|---------|
| `install.sh` | Install scx-scheds/scx-tools + build/install GUI |
| `update.sh` | Update scx-scheds/scx-tools + rebuild GUI |
| `uninstall.sh` | Remove scx-switcher only |
| `build.sh` | Build the GUI binary |
| `build-deb.sh` | Build the Debian/Ubuntu .deb package |

`install.sh` options: `--resume`, `--skip-gui`, `--skip-deps`,
`--from-local=DIR`, `--dry-run`, `--os-release=FILE`.

## Credits

SCX Switcher was inspired by:
- [lgl-scxctl-manager](https://github.com/linuxgamerlife/lgl-scxctl-manager)
- [scx-tool](https://github.com/xerolinux/scx-tool)
- [scx-manager](https://github.com/CachyOS/scx-manager)

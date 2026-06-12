# SCX Switcher

Qt6 GUI for managing sched-ext BPF CPU schedulers on **Debian 13+ (Trixie)**.

## Dependencies

- [scx-scheds](https://github.com/hmwassim/scx-bundler/releases)
- [scx-tools](https://github.com/hmwassim/scx-bundler/releases)
- Kernel 6.12+ with sched-ext support

## Install

```sh
# Download the latest .debs from the releases page above, then:
sudo apt install ./scx-scheds_*.deb ./scx-tools_*.deb ./scx-switcher_*.deb
```

Or use the install script (auto-downloads .debs, builds GUI from source):

```sh
./install.sh
```

## Scripts

| Script | Purpose |
|--------|---------|
| `install.sh` | Download latest .debs, build & install GUI |
| `update.sh` | Redownload .debs + rebuild GUI |
| `uninstall.sh` | Remove scx-switcher only |
| `build.sh` | Build the GUI binary |
| `build-deb.sh` | Build the .deb package |

## Build

```sh
sudo apt install dpkg-dev debhelper cmake qt6-base-dev libgl-dev g++
./build-deb.sh
```

# SCX Switcher

Qt6 GUI for switching [sched_ext](https://github.com/sched-ext/scx) CPU
schedulers. It shows the running scheduler, starts, switches or stops one, and
saves your choice as the scx_loader boot default. It also lives in the tray.

## Requirements

- Linux 6.12+ with `CONFIG_SCHED_CLASS_EXT`
- `scx-scheds` and `scx-tools` (`scxctl`, `scx_loader`, systemd).
  On Debian, build them with [scx-bundler](https://github.com/hmwassim/scx-bundler).
- polkit with `pkexec` and an authentication agent
- To build: CMake, a C++20 compiler and Qt 6.4+ (`sudo apt install cmake g++ qt6-base-dev`)

## Build and install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Or build a .deb:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
(cd build && cpack)
sudo apt install ./build/scx-switcher_*.deb
```

To uninstall a `cmake --install`: `sudo xargs rm < build/install_manifest.txt`.

## How it works

The GUI reads state as your user, and trusts the kernel over scx_loader:

- `/sys/kernel/sched_ext/state` says whether a scheduler is actually attached.
  scx_loader keeps reporting a scheduler after it has crashed, so when the two
  disagree the header shows the scheduler as failed.
- `scxctl get` / `scxctl list` give the scheduler name, mode and the list of
  schedulers scx_loader supports.
- `/etc/scx_loader/config.toml` gives the boot default shown under the status.

Changes go through one root helper,
`/usr/libexec/scx-switcher/scx-switcher-helper`, run with `pkexec`:

- `apply SCHED MODE`: starts or switches the scheduler, then waits for the
  kernel to confirm it's attached. Only then does it save it as the boot
  default. If the scheduler dies, the error and the scx_loader journal are
  shown and the boot default is left unchanged.
- `stop`: stops the scheduler and removes the boot default.
- `repair`: offered when scx_loader won't start. It clears the boot default
  and restarts scx_loader. If the config is still invalid, it is moved to
  `config.toml.broken` and replaced with a fresh one.

The helper only touches `default_sched` and `default_mode` and never leaves
the config empty (scx_loader refuses to start on an empty file). It also only
accepts scheduler names that scx_loader lists, since an unknown name stops
scx_loader from starting at boot.

A systemd drop-in skips `scx_loader.service` on kernels without sched_ext.

## Credits

Inspired by [lgl-scxctl-manager](https://github.com/linuxgamerlife/lgl-scxctl-manager),
[scx-tool](https://github.com/xerolinux/scx-tool) and
[scx-manager](https://github.com/CachyOS/scx-manager).

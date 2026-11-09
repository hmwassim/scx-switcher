# SCX Switcher

A small Qt window and tray icon for [sched_ext](https://github.com/sched-ext/scx) CPU
schedulers on Debian 13. It shows which scheduler is running, starts, stops or switches one,
picks a mode for it, and lets you decide whether it starts at boot.

It runs schedulers through `scx.service` and `/etc/default/scx`, which ship with the
`scx-scheds` package from [scx-bundler](https://github.com/hmwassim/scx-bundler). It doesn't
need scx_loader.

## Requirements

- A kernel with `CONFIG_SCHED_CLASS_EXT` (Linux 6.12 or newer; Debian's own kernels have it)
- `scx-scheds` from scx-bundler
- polkit with `pkexec` and an authentication agent (every desktop session has one)

## Build

Requires Docker.

```sh
./build.sh
sudo apt install ./out/scx-switcher_*.deb
```

The package is built in a clean `debian:trixie` container with `dpkg-buildpackage` and
checked with `lintian`.

## How it works

- **The kernel says what runs.** The app reads `/sys/kernel/sched_ext/state`, `root/ops` and
  `enable_seq`: every 2 seconds while the window is open, every 5 seconds in tray-only mode,
  and not at all otherwise. `systemctl` is only asked when something changes.
- **One root helper makes changes.** `/usr/libexec/scx-switcher/scx-switcher-helper` runs
  through `pkexec`:
  - `start SCHED on|off [OPTIONS...]` writes `/etc/default/scx`, restarts `scx.service` and
    waits for the kernel to confirm. If the scheduler fails, the previous one comes back.
  - `boot on|off` turns boot start on or off.
  - `stop` stops the scheduler now; boot start stays as it is.
  - `stop-loader` stops scx_loader when it holds the scheduler, since only one can run.

  It accepts only installed `scx_*` names and options made of letters, digits and
  `- _ = . , : / +`.
- **Modes are option presets.** They are the ones scx_loader ships. The window shows the
  exact command each one runs. Custom… takes your own options.
- **Nothing starts at boot unless you tick "Start at boot".** The app itself only starts at
  login if you tick "Start at login" in the tray menu. It then runs tray-only and never builds
  the window until you open it.

If a scheduler ever hangs the desktop, **Alt+SysRq+S** returns to the kernel default right
away. The kernel also does this by itself when a scheduler stalls for too long.

## License

MIT. Scheduler groups and descriptions follow sched-ext/scx (`services/catalog.json` and each
scheduler's README); mode presets follow scx_loader.

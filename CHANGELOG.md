# Changelog

All notable changes to SCX Switcher will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.3] - 2026-07-07

### Fixed
- Fix PrivOps kill()+start() reentrancy bug — pending operations are now
  queued and drained on the finished signal instead of being silently dropped
- Fix errorOccurred(FailedToStart) not clearing pending operations
- Fix install.sh shellcheck warnings (unquoted variable in glob patterns)

### Added
- Add startup check that scxctl PATH matches PolKit policy exec.path;
  warns in log pane on mismatch
- Document why Qt6::Network is needed (QLocalServer/QLocalSocket)

### Security
- Add PolKit exec.path verification for scxctl at startup

## [1.1.2] - 2026-07-06

### Added
- Added support for 7 new schedulers in ALL_SCHEDULERS: cake, chaos, flow,
  forge, mitosis, pandemonium, rlfifo (Reference tab + mode selection)
- Added RLFIFO display name override in humanizeSched()

### Removed
- Removed nest and simple from scheduler list (no longer in upstream scx)

## [1.1.0] - 2026-06-13

### Fixed
- Removed broken marquee (replaced with plain setText)

### Changed
- Updated cmake configuration to use Release build type by default

## [1.0.0] - Initial Release

- First public release of SCX Switcher GUI for managing sched-ext CPU schedulers

# Changelog

All notable project changes are recorded here. Dates use the Asia/Baku project
timezone.

## 2026-08-16

### Added

- `antivermis`, a read-only Linux/macOS threat-hunting scanner written in C.
- Streaming SHA-256 and optional local signature-database matching.
- Explainable rules for compound miner/Stratum indicators, download-and-execute
  droppers, risky temporary executables, set-ID and world-writable executables,
  persistence entries, and Linux loader-preload/rootkit indicators.
- System-location scanning through `antivermis --system`.

### Security

- Bounded files, recursion, total bytes, findings, and signature records.
- Symlinks, FIFOs, devices, and sockets are not followed or read.
- Files and directories are opened without following symlinks and revalidated
  by device/inode to reduce replacement races.
- No automatic deletion, quarantine, process killing, credential access, hash
  uploads, or claims of guaranteed rootkit detection.

## 2026-08-15

### Added — cross-platform system utilities

- `coreinfo` — CPU, memory, architecture, page-size, and kernel summary.
- `clockres` — POSIX clock-resolution inspection.
- `numconv` — checked decimal/hexadecimal/octal/binary conversion.
- `linkscan` — bounded hard-link discovery by device and inode.
- `autostartx` — read-only Linux/macOS startup-location inventory.
- `whoisx` — WHOIS client with validated input, bounded output, and connection,
  send, and receive timeouts.
- Sysinternals/NirSoft-to-pydev capability mapping and explicit exclusion of
  password/credential extraction utilities.

### Added — Linux administration suite

- `traceflow` — Linux x86-64 syscall and descendant-process tree tracing.
- `syswatch` — CPU, memory, load, network, and disk dashboard with separate CSV
  logs and safe SAR-text viewing.
- `sessionx` — login/process mapping with dry-run-first session termination.
- `procexp` — process ownership, memory, thread, executable, and descriptor view.

### Security

- Root, PID 1, and the caller are protected from `sessionx` termination.
- Process ownership is revalidated immediately before SIGTERM.
- Monitoring logs and SAR input reject symlink targets.
- Process environments are intentionally excluded from reports.

## 2026-08-11

### Added

- `checksum`, `hexview`, `stringsx`, `randpass`, `syscallx`, and `lsx`.
- Optional SQLite detection, including an explicit no-SQLite build/test path.
- GitHub Actions quality checks for strict builds, functional tests, the
  SQLite fallback, AddressSanitizer, and UndefinedBehaviorSanitizer.

### Fixed and hardened

- Full JSON grammar validation, nesting/output limits, and malformed-input tests.
- Linear bounded HTML link parsing.
- Symbolic-link-safe writing and special-file rejection.
- Oversized-input, random-input, injection, and recursive-size tests.

## 2026-08-09

### Added

- Converted the repaired Python sketches into native C11 command-line tools.
- Added shared bounded readers, strict compiler warnings, Make targets, and
  end-to-end tests.
- Added the GitHub Pages project site and automated Pages deployment.

### Fixed

- Removed device-specific Android paths and import-time side effects.
- Completed the Codebreaker and tic-tac-toe logic.
- Replaced unsafe SQL construction with prepared statements.
- Replaced shell-based package launching with direct `execvp` argument passing.

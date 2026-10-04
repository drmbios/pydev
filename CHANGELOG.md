# Changelog

All notable project changes are recorded here. Dates use the Asia/Baku project
timezone.

## 2026-10-04

### Fixed and hardened

- Enforced C/Python Antivermis byte limits while files grow, bounded C directory
  visits, retained admitted files during truncated Python scans, and aligned
  JSON exit codes with findings and incomplete coverage.
- Anchored recursive C scans to directory descriptors and changed SAR input to
  validate the opened descriptor without a blocking special-file race. SAR
  byte accounting now includes NUL bytes, and C CPU totals exclude duplicate
  guest counters.
- Suppressed hex previews containing detected secrets and redacted private-key
  bodies. Bounded startup enumeration and preserved partial results when a
  startup file is too large.
- Rechecked process identity before publishing metadata, prevented missing-file
  correlations with unknown executable paths, and propagated partial workflow
  coverage. Rejected malformed, negative, boolean, and non-finite telemetry.
- Fixed largest-first directory sorting and avoided a second recursive size
  scan. Codebreaker now accepts only ASCII digits and uses system randomness.
- Rejected HTTPS update downgrades before following redirects and prevented
  remote Python signature manifests from reading local database URLs.
- Added security regressions, a vulnerability-reporting policy, Dependabot
  configuration, dependency review, commit-pinned Actions, and restricted Pages
  deployment to the default branch.

Repository-side controls were enabled separately: CodeQL extended queries,
secret scanning/push protection, vulnerability alerts, Dependabot security
updates, private reporting, read-only default workflow tokens, and protected
`master`. See `docs/SECURITY_REVIEW.md` for verification and alert-triage limits.

## 2026-09-13

### Added

- `pydev_ai`, a versioned structured JSON adapter with explicit evidence,
  coverage, capabilities, status, units, timestamps, and structured errors.
- Deterministic system-pressure, file-indicator, and AI-worker-health workflows.
- Optional official MCP Python SDK v2 stdio server with twelve read-only tools,
  strict filesystem roots, concurrency/work/response limits, and no shell,
  mutation, or network tools.
- Optional NVIDIA and explicitly configured inference-service telemetry that
  reports unsupported or missing measurements without fabricated values.
- Contract, metric, boundary, prompt-injection, timeout, counter-reset, PID
  reuse, and real MCP session tests.

### Fixed and hardened

- Correct delta-based Linux CPU, process, network, and disk rates, including
  guest-time handling, PID identity, counter resets, interface churn, and
  top-level block-device accounting.
- Added structured JSON modes to selected existing C/Python commands and a
  stable, machine-discoverable Antivermis rule inventory.
- Rooted file access now rejects symlink escapes and blocking special files;
  previews redact likely secrets and treat collected text as untrusted data.

## 2026-08-23

### Added

- Independent Python 3.9+ counterparts for all previously C-only tools:
  `checksum`, `hexview`, `stringsx`, `randpass`, `syscallx`, `lsx`, `traceflow`,
  `syswatch`, `sessionx`, `procexp`, `coreinfo`, `clockres`, `numconv`,
  `linkscan`, `autostartx`, `whoisx`, and `antivermis`.
- Shared bounded regular-file helpers in `pycommon.py` and `make python-check`
  for syntax and regression testing in CI.
- Node 24-compatible GitHub Actions majors for CI and Pages deployment.

### Security

- Python counterparts preserve bounded reads, symlink avoidance, cryptographic
  randomness, read-only defaults, guarded session termination, hidden process
  environments, and hash-verified atomic Antivermis database updates.

## 2026-08-19

### Added

- Opt-in `antivermis --check-update` and `--update-db` commands for bounded
  HTTPS signature manifests, with an offline `file://` mode for testing.

### Security

- Updated databases are size-bounded, SHA-256 verified, parser-validated,
  written with user-only permissions, and atomically installed without
  following a destination symlink. Malware bodies and samples are never
  downloaded.

## 2026-08-16

### Added

- `antivermis`, a read-only Linux/macOS threat-hunting scanner written in C.
- Streaming SHA-256 and optional local signature-database matching.
- Built-in harmless EICAR test-file recognition and ClamAV-compatible SHA-256
  `HashString:FileSize:MalwareName` database records.
- Added documented validation references for EICAR, ClamAV, MITRE ATT&CK
  Resource Hijacking, and the current YARA/archive-scanning boundary.
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

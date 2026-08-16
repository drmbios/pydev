# pydev native tools

Small Linux-style command-line tools converted from the original Python sketches.
Their implementations are standard C11; `sql` links to the system SQLite C library.

The repository keeps the repaired Python editions as readable reference
implementations and provides hardened native C versions for everyday terminal use.

## Requirements

- A C11 compiler (`cc`, GCC, or Clang)
- POSIX-compatible system (Linux, macOS, or BSD)
- SQLite 3 development library for full `sql` functionality (optional)
- Python 3 and pip only when using `qpipper`

## Build

Build and test:

```sh
make
make check
make sanitize
```

Executables are written to `bin/`:

- `cntr FILE` — case-insensitive ASCII letter frequencies
- `codebreaker [ATTEMPTS]` — four-digit code game
- `parser_html FILE` — extract quoted `href` values
- `qpipper {search|install} PACKAGE` — shell-free pip launcher
- `readjson FILE` / `rdr2dot0 FILE` — bounded JSON viewers
- `sql DATABASE {list|add NAME ANSWER}` — SQLite-backed answer store
- `contacts FILE.vcf` — vCard contact summary (replaces the vague `test.py` name)
- `ttt` — two-player tic-tac-toe
- `write2file FILE TEXT` — bounded text writer
- `checksum FILE...` — CRC-32 checksums for integrity comparisons
- `hexview FILE [MAX_BYTES]` — bounded hexadecimal and ASCII file view
- `stringsx FILE [MIN_LENGTH]` — extract printable strings from binary files
- `randpass [LENGTH]` — securely generate an unbiased random password
- `syscallx NUMBER...` — translate Linux x86-64 syscall numbers into readable explanations
- `lsx [-aSr] [PATH]` — list entries with numeric permissions and recursive real sizes
- `traceflow COMMAND [ARG...]` — trace syscall and child-process dependencies as a tree (Linux x86-64)
- `syswatch [--interval SEC] [--count N] [--log DIR]` — combined CPU, memory, load, disk, and network dashboard (Linux)
- `sessionx [--terminate USER [--confirm]]` — list login sessions and owned processes, with guarded termination (Linux)
- `procexp [PID]` — process explorer with ownership, memory, threads, executable, and descriptor counts (Linux)
- `coreinfo` — hardware, kernel, CPU, page-size, and physical-memory summary
- `clockres` — show the resolution of available POSIX system clocks
- `numconv NUMBER` — safely convert decimal, hexadecimal, octal, and binary integers
- `linkscan FILE [ROOT]` — find every hard link sharing a file's device and inode
- `autostartx` — inventory standard Linux and macOS startup locations without executing entries
- `whoisx [--server HOST] NAME` — bounded WHOIS client with DNS and socket timeouts
- `antivermis [OPTIONS] PATH...` — read-only malware, miner, persistence, and rootkit-indicator scanner

`syscallx` reads the host's Linux x86-64 `unistd_64.h` table when available,
giving it coverage of the syscalls supported by the installed kernel headers.
Its built-in core table keeps common numbers such as `2` (`open`) available on
development machines that do not ship Linux headers.

`lsx` prints permissions as octal numbers such as `700` and `755`, classifies
entry types, and totals directory contents recursively using B/KB/MB/GB/TB/PB
units. Use `-a` for hidden entries, `-S` for largest-first sorting, and `-r` to
reverse the order. Symbolic links are measured but never followed.

`traceflow` launches and traces a command, follows fork/vfork/clone/exec events,
and renders syscall activity beneath each child process. It deliberately does
not attach to arbitrary running processes. `syswatch` reads bounded Linux
`/proc` metrics, draws terminal bars, and can append separate `cpu.csv`,
`memory.csv`, `network.csv`, `disk.csv`, and `load.csv` files. Use
`syswatch --sar FILE` to safely view exported text from `sar`; binary sysstat
archives must first be converted with the system `sar` command.

`sessionx` shows interactive logins and processes owned by each account. A
termination request is a dry run unless `--confirm` is supplied. It always
refuses root, PID 1, and itself, and rechecks process ownership immediately
before sending SIGTERM. `procexp` provides a compact process inventory and
detailed PID view while intentionally omitting environment variables, which
often contain API keys and other secrets.

## Unix utility-suite mapping

The project provides Unix-native equivalents inspired by the troubleshooting
categories in the Sysinternals Suite and NirSoft utility catalog. They are not
copies of Windows APIs or user interfaces:

| Windows utility family | pydev terminal equivalent |
| --- | --- |
| Process Monitor / ProcDump | `traceflow`, `syswatch` |
| Process Explorer / PsList | `procexp`, `sessionx` |
| Coreinfo / ClockRes | `coreinfo`, `clockres` |
| Hex2dec | `numconv` |
| FindLinks / Disk Usage | `linkscan`, `lsx` |
| Autoruns | `autostartx` |
| Whois | `whoisx` |
| Strings / file inspection | `stringsx`, `hexview`, `checksum` |
| Threat hunting | `antivermis` |

Credential and password-recovery utilities are intentionally excluded: pydev
does not extract browser passwords, wireless keys, authentication tokens, or
operating-system credential stores.

## Antivermis threat scanner

`antivermis` is a pure-C, read-only threat-hunting scanner for Linux and macOS.
It streams regular files through SHA-256, optionally checks a local signature
database, and reports explainable indicators such as compound Stratum/miner
strings, download-and-execute scripts, risky executables in temporary storage,
set-id or world-writable executables, unsafe persistence entries, and non-empty
Linux loader-preload configuration.

```sh
bin/antivermis ~/Downloads /tmp
bin/antivermis --system
bin/antivermis --db signatures.txt ~/Downloads
```

Signature database lines contain a 64-character SHA-256 digest, whitespace,
and a short label. Scans default to 100,000 files, 32 MiB per file, 1 GiB total,
64 directory levels, and 10,000 findings. `--max-files N` and
`--max-bytes MiB` can lower or raise selected limits within hard ceilings.
Symlinks, FIFOs, devices, and sockets are never followed or read.

Exit status `0` means a complete scan with no findings, `1` means findings were
reported, and `2` means invalid configuration or incomplete coverage caused by
errors or limits. Antivermis does not delete, quarantine, kill, upload hashes,
read credential stores, or promise that a machine is clean. Kernel rootkits can
hide evidence from user-space tools; rootkit rules therefore report indicators
for manual verification rather than definitive infection claims.

The build automatically detects SQLite's header and library together. When
they are unavailable, every other tool still builds and `sql --backend`
explains that SQLite support is disabled. Run `make check-no-sqlite` to verify
the fallback build explicitly.

## What changed

- Removed device-specific `/sdcard/qpython` paths and made every input explicit.
- Reworked the Python programs into import-safe, testable command-line apps.
- Added native C11 implementations with strict compiler warnings.
- Completed the Codebreaker and tic-tac-toe game logic.
- Replaced unsafe SQL construction with prepared statements.
- Replaced the obsolete in-process pip API and shell execution with `execvp`.
- Added bounded HTML, JSON, text, and vCard readers.
- Added checksum, binary inspection, string extraction, and secure password tools.
- Added syscall translation and recursive numeric-permission directory listing.
- Added dependency-tree syscall tracing, unified system monitoring, guarded
  session management, and Sysinternals-style process inspection.
- Added cross-platform hardware, clock, numeric-conversion, hard-link,
  autostart, and WHOIS utilities.
- Added `antivermis` with streaming SHA-256 signatures and bounded,
  explainable threat indicators.
- Added repeatable functional, oversized-input, and injection-resistance tests.

See [CHANGELOG.md](CHANGELOG.md) for the dated August 9–16 development history.

## Safety and resource limits

Security limits:

- File readers reject inputs larger than 16 MiB.
- File readers reject FIFOs and other blocking special inputs.
- JSON nesting is capped at 128 levels.
- JSON is grammar-validated before output and formatted output is capped at 64 MiB.
- Interactive input and database fields have explicit limits.
- `qpipper` calls `execvp` directly and never invokes a shell.
- `write2file` refuses symbolic-link targets and closes descriptors on every path.
- Runtime process scans and trace tracking have hard entry limits.
- Monitoring logs refuse symbolic-link files and are created with user-only permissions.
- Session termination is dry-run by default and cannot target root, PID 1, or the caller.
- `antivermis` never follows symlinks or reads special files, revalidates opened
  objects, and caps per-file, total-byte, recursion, signature, and finding work.
- The sanitizer target checks address, leak, and undefined-behavior errors where
  supported by the host compiler.

Run `make check` for end-to-end tests and `make sanitize` for the instrumented
memory-safety build. Build artifacts are ignored by Git and can be removed with
`make clean`.

## License

Licensed under the repository's [Apache License 2.0](LICENSE).

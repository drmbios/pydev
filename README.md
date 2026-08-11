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

`syscallx` reads the host's Linux x86-64 `unistd_64.h` table when available,
giving it coverage of the syscalls supported by the installed kernel headers.
Its built-in core table keeps common numbers such as `2` (`open`) available on
development machines that do not ship Linux headers.

`lsx` prints permissions as octal numbers such as `700` and `755`, classifies
entry types, and totals directory contents recursively using B/KB/MB/GB/TB/PB
units. Use `-a` for hidden entries, `-S` for largest-first sorting, and `-r` to
reverse the order. Symbolic links are measured but never followed.

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
- Added repeatable functional, oversized-input, and injection-resistance tests.

## Safety and resource limits

Security limits:

- File readers reject inputs larger than 16 MiB.
- File readers reject FIFOs and other blocking special inputs.
- JSON nesting is capped at 128 levels.
- JSON is grammar-validated before output and formatted output is capped at 64 MiB.
- Interactive input and database fields have explicit limits.
- `qpipper` calls `execvp` directly and never invokes a shell.
- `write2file` refuses symbolic-link targets and closes descriptors on every path.
- The sanitizer target checks address, leak, and undefined-behavior errors where
  supported by the host compiler.

Run `make check` for end-to-end tests and `make sanitize` for the instrumented
memory-safety build. Build artifacts are ignored by Git and can be removed with
`make clean`.

## License

Licensed under the repository's [Apache License 2.0](LICENSE).

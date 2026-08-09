# pydev native tools

Small Linux-style command-line tools converted from the original Python sketches.
Their implementations are standard C11; `sql` links to the system SQLite C library.

The repository keeps the repaired Python editions as readable reference
implementations and provides hardened native C versions for everyday terminal use.

## Requirements

- A C11 compiler (`cc`, GCC, or Clang)
- POSIX-compatible system (Linux, macOS, or BSD)
- SQLite 3 development library for the `sql` utility
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

## What changed

- Removed device-specific `/sdcard/qpython` paths and made every input explicit.
- Reworked the Python programs into import-safe, testable command-line apps.
- Added native C11 implementations with strict compiler warnings.
- Completed the Codebreaker and tic-tac-toe game logic.
- Replaced unsafe SQL construction with prepared statements.
- Replaced the obsolete in-process pip API and shell execution with `execvp`.
- Added bounded HTML, JSON, text, and vCard readers.
- Added repeatable functional, oversized-input, and injection-resistance tests.

## Safety and resource limits

Security limits:

- File readers reject inputs larger than 16 MiB.
- JSON nesting is capped at 128 levels.
- Interactive input and database fields have explicit limits.
- `qpipper` calls `execvp` directly and never invokes a shell.
- The sanitizer target checks address, leak, and undefined-behavior errors where
  supported by the host compiler.

Run `make check` for end-to-end tests and `make sanitize` for the instrumented
memory-safety build. Build artifacts are ignored by Git and can be removed with
`make clean`.

## License

Licensed under the repository's [Apache License 2.0](LICENSE).

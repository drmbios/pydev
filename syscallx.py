"""Translate Linux x86-64 syscall numbers into readable names."""

from __future__ import annotations

import re
import sys
from pathlib import Path

CORE = {0: ("read", "read from a file descriptor"), 1: ("write", "write to a file descriptor"),
        2: ("open", "open a file"), 3: ("close", "close a file descriptor"),
        39: ("getpid", "get the current process ID"), 57: ("fork", "create a child process"),
        59: ("execve", "execute a program"), 60: ("exit", "terminate the process"),
        62: ("kill", "send a signal to a process"), 231: ("exit_group", "terminate all process threads")}
HEADER_LIMIT = 4 * 1024 * 1024


def load_table(path: Path | None = None) -> dict[int, tuple[str, str]]:
    table = dict(CORE)
    candidates = [path] if path else [Path("/usr/include/x86_64-linux-gnu/asm/unistd_64.h"), Path("/usr/include/asm/unistd_64.h")]
    for candidate in candidates:
        if candidate and candidate.is_file() and candidate.stat().st_size <= HEADER_LIMIT:
            for line in candidate.read_text("ascii", errors="ignore").splitlines():
                match = re.fullmatch(r"#define\s+__NR_([A-Za-z0-9_]+)\s+(\d+)", line.strip())
                if match:
                    table[int(match.group(2))] = (match.group(1), "Linux system call")
            break
    return table


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    header = None
    if len(args) >= 2 and args[0] == "--table":
        header, args = Path(args[1]), args[2:]
    if not args:
        print("usage: syscallx.py [--table LINUX_UNISTD_HEADER] NUMBER...", file=sys.stderr)
        return 2
    table, failed = load_table(header), False
    for text in args:
        try:
            number = int(text, 10)
            name, description = table[number]
            print(f"{number}: {name} — {description}")
        except (ValueError, KeyError):
            print(f"{text}: unknown syscall", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

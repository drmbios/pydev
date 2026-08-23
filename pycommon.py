"""Shared bounded-file helpers for the Python command-line tools."""

from __future__ import annotations

import os
import stat
from pathlib import Path

MAX_INPUT = 16 * 1024 * 1024


def regular_file(path: str | os.PathLike[str], limit: int = MAX_INPUT) -> tuple[int, os.stat_result]:
    """Open a regular file without following its final symlink."""
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags)
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise ValueError(f"input must be a regular file no larger than {limit} bytes")
        return descriptor, info
    except Exception:
        os.close(descriptor)
        raise


def read_bytes(path: str | os.PathLike[str], limit: int = MAX_INPUT) -> bytes:
    descriptor, before = regular_file(path, limit)
    try:
        chunks: list[bytes] = []
        total = 0
        while True:
            chunk = os.read(descriptor, min(65_536, limit + 1 - total))
            if not chunk:
                break
            chunks.append(chunk)
            total += len(chunk)
            if total > limit:
                raise ValueError("input grew beyond the configured limit")
        after = os.fstat(descriptor)
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns
        ):
            raise OSError("input changed while being read")
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def safe_text(value: object) -> str:
    return str(value).encode("unicode_escape", "backslashreplace").decode("ascii")


def path_is_within(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except (OSError, ValueError):
        return False

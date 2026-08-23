"""Directory listing with octal permissions and recursive real sizes."""

from __future__ import annotations

import argparse
import os
import stat
from pathlib import Path


def human_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KB", "MB", "GB", "TB", "PB"):
        if value < 1024 or unit == "PB":
            return f"{int(value)} B" if unit == "B" else f"{value:.1f} {unit}"
        value /= 1024
    raise AssertionError


def real_size(path: Path, seen: set[tuple[int, int]] | None = None) -> int:
    seen = set() if seen is None else seen
    total, entries = 0, 0
    stack = [(path, 0)]
    while stack:
        current, depth = stack.pop()
        try: info = current.lstat()
        except OSError: continue
        key = (info.st_dev, info.st_ino)
        if key in seen: continue
        seen.add(key); entries += 1
        if entries > 100_000: raise RuntimeError("entry limit exceeded")
        if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode): total += info.st_size; continue
        total += info.st_size
        if depth >= 64: raise RuntimeError("directory depth limit exceeded")
        try:
            with os.scandir(current) as children:
                for child in children:
                    if not child.is_symlink(): stack.append((Path(child.path), depth + 1))
                    else: total += child.stat(follow_symlinks=False).st_size
        except OSError: pass
    return total


def list_entries(path: Path, hidden: bool = False, reverse: bool = False, size_sort: bool = False) -> list[tuple[str, int, str, str]]:
    candidates = [path] if not path.is_dir() else [Path(entry.path) for entry in os.scandir(path) if hidden or not entry.name.startswith(".")]
    rows = []
    for item in candidates:
        info = item.lstat()
        kind = "dir" if stat.S_ISDIR(info.st_mode) else "link" if stat.S_ISLNK(info.st_mode) else "file"
        rows.append((item.name, stat.S_IMODE(info.st_mode), kind, human_size(real_size(item))))
    rows.sort(key=(lambda row: real_size(path / row[0])) if size_sort and path.is_dir() else (lambda row: row[0]), reverse=reverse)
    return rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", action="store_true"); parser.add_argument("-S", action="store_true"); parser.add_argument("-r", action="store_true")
    parser.add_argument("path", nargs="?", default=".")
    args = parser.parse_args(argv)
    try:
        for name, mode, kind, size in list_entries(Path(args.path), args.a, args.r, args.S):
            print(f"{mode:03o}  {kind:4}  {size:>10}  {name}")
        return 0
    except (OSError, RuntimeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())

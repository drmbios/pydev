"""Find hard links without following symbolic links."""

from __future__ import annotations

import os
import sys
from pathlib import Path

MAX_ENTRIES = 100_000


def find_links(target: Path, root: Path) -> list[Path]:
    wanted = target.lstat()
    matches, count = [], 0
    for directory, names, files in os.walk(root, followlinks=False):
        names[:] = [name for name in names if not Path(directory, name).is_symlink()]
        for name in names + files:
            count += 1
            if count > MAX_ENTRIES:
                raise RuntimeError("entry limit exceeded")
            path = Path(directory, name)
            try:
                info = path.lstat()
                if (info.st_dev, info.st_ino) == (wanted.st_dev, wanted.st_ino):
                    matches.append(path)
            except OSError:
                continue
    return matches


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if not 1 <= len(args) <= 2:
        print("usage: linkscan.py FILE [SEARCH_ROOT]", file=sys.stderr); return 2
    try:
        for path in find_links(Path(args[0]), Path(args[1]) if len(args) == 2 else Path(".")):
            print(path)
        return 0
    except (OSError, RuntimeError) as error:
        print(f"linkscan: {error}", file=sys.stderr); return 1


if __name__ == "__main__":
    raise SystemExit(main())

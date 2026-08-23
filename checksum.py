"""Streaming CRC-32 checksum utility."""

from __future__ import annotations

import sys
import zlib
from pycommon import read_bytes


def crc32_file(path: str) -> int:
    return zlib.crc32(read_bytes(path)) & 0xFFFFFFFF


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if not args:
        print("usage: checksum.py FILE...", file=sys.stderr)
        return 2
    failed = False
    for path in args:
        try:
            print(f"{crc32_file(path):08x}  {path}")
        except (OSError, ValueError) as error:
            print(f"checksum: {path}: {error}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

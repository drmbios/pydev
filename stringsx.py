"""Extract bounded printable ASCII strings from a regular file."""

from __future__ import annotations

import sys
from pycommon import read_bytes


def strings(data: bytes, minimum: int = 4) -> list[str]:
    found, current = [], bytearray()
    for byte in data + b"\0":
        if 32 <= byte < 127:
            current.append(byte)
        else:
            if len(current) >= minimum:
                found.append(current.decode("ascii"))
            current.clear()
    return found


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    try:
        if not 1 <= len(args) <= 2:
            raise ValueError
        minimum = int(args[1]) if len(args) == 2 else 4
        if not 1 <= minimum <= 4096:
            raise ValueError
        for value in strings(read_bytes(args[0]), minimum):
            print(value)
        return 0
    except ValueError:
        print("usage: stringsx.py FILE [MIN_LENGTH:1-4096]", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"stringsx: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

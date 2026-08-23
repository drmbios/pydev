"""Bounded hexadecimal and ASCII file viewer."""

from __future__ import annotations

import sys
from pycommon import MAX_INPUT, read_bytes


def format_hex(data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        hexadecimal = " ".join(f"{byte:02x}" for byte in chunk).ljust(47)
        printable = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        rows.append(f"{offset:08x}  {hexadecimal}  |{printable}|")
    return "\n".join(rows)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    try:
        if not 1 <= len(args) <= 2:
            raise ValueError
        limit = int(args[1]) if len(args) == 2 else MAX_INPUT
        if not 1 <= limit <= MAX_INPUT:
            raise ValueError
        data = read_bytes(args[0], MAX_INPUT)
        print(format_hex(data[:limit]))
        if len(data) > limit:
            print(f"hexview: output limited to {limit} of {len(data)} bytes", file=sys.stderr)
        return 0
    except ValueError:
        print(f"usage: hexview.py FILE [MAX_BYTES:1-{MAX_INPUT}]", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"hexview: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

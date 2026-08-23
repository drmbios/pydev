"""Integer-base converter with signed 64-bit bounds."""

from __future__ import annotations

import sys


def convert(text: str) -> dict[str, str]:
    if text.startswith("-"):
        raise ValueError("negative values are not supported")
    base = 16 if text.lower().startswith("0x") else 8 if len(text) > 1 and text.startswith("0") else 10
    value = int(text, base)
    if not 0 <= value <= (1 << 64) - 1:
        raise OverflowError("outside unsigned 64-bit range")
    return {"decimal": str(value), "hex": f"0x{value:x}", "octal": f"0{value:o}", "binary": f"{value:b}"}


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 1:
        print("usage: numconv.py NUMBER", file=sys.stderr)
        return 2
    try:
        for name, value in convert(args[0]).items():
            print(f"{name}: {value}")
        return 0
    except (ValueError, OverflowError) as error:
        print(f"numconv: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

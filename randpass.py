"""Cryptographically secure password generator."""

from __future__ import annotations

import secrets
import sys

ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%_-+=?"


def generate(length: int = 24) -> str:
    if not 8 <= length <= 4096:
        raise ValueError("length must be between 8 and 4096")
    return "".join(secrets.choice(ALPHABET) for _ in range(length))


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    try:
        if len(args) > 1:
            raise ValueError
        print(generate(int(args[0]) if args else 24))
        return 0
    except ValueError:
        print("usage: randpass.py [LENGTH:8-4096]", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

"""Write text to a file."""

import argparse
import os
from pathlib import Path

MAX_TEXT_SIZE = 1024 * 1024


def write_text(path: Path, text: str) -> None:
    data = text.encode("utf-8")
    if len(data) > MAX_TEXT_SIZE:
        raise ValueError("text exceeds the 1 MiB limit")
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags, 0o600)
    try:
        written = 0
        while written < len(data):
            amount = os.write(descriptor, data[written:])
            if amount <= 0: raise OSError("short write")
            written += amount
    finally: os.close(descriptor)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("text", nargs="?", help="text to write; prompts when omitted")
    args = parser.parse_args()
    text = args.text if args.text is not None else input("Write some text: ")
    write_text(args.file, text)
    print(f"File {args.file} was saved.")


if __name__ == "__main__":
    main()

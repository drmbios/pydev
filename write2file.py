"""Write text to a file."""

import argparse
from pathlib import Path


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


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

"""Count ASCII letters in a text file."""

import argparse
import string
from collections import Counter
from pathlib import Path
from typing import Dict


def count_letters(text: str) -> Dict[str, int]:
    counts = Counter(text)
    return {letter: counts[letter] for letter in string.ascii_letters}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="text file to inspect")
    args = parser.parse_args()

    text = args.file.read_text(encoding="utf-8")
    for letter, count in count_letters(text).items():
        print(letter, count)


if __name__ == "__main__":
    main()

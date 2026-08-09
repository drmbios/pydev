"""Read a JSON file and print its top-level items."""

import argparse
import json
from pathlib import Path
from typing import Any


def read_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as file:
        return json.load(file)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    value = read_json(args.file)
    items = value.items() if isinstance(value, dict) else value
    for item in items:
        print(item)


if __name__ == "__main__":
    main()

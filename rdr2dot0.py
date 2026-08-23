"""Read and display a JSON or JSON Lines file."""

import argparse
import json
from pathlib import Path
from typing import Any
from pycommon import read_bytes


def read_json(path: Path) -> Any:
    text = read_bytes(path).decode("utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return [json.loads(line) for line in text.splitlines() if line.strip()]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    print(json.dumps(read_json(args.file), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()

"""Read and display a JSON or JSON Lines file."""

import argparse
import json
from pathlib import Path
from typing import Any


def read_json(path: Path) -> Any:
    text = path.read_text(encoding="utf-8")
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

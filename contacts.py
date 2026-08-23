"""Extract names and telephone numbers from a bounded vCard file."""

from __future__ import annotations

import argparse
from pathlib import Path
from pycommon import read_bytes


def parse_contacts(text: str) -> list[dict[str, object]]:
    contacts: list[dict[str, object]] = []; current: dict[str, object] | None = None
    for raw_line in text.splitlines()[:100_000]:
        line = raw_line.strip()
        if line.upper() == "BEGIN:VCARD": current = {"name": "(unnamed)", "phones": []}
        elif line.upper() == "END:VCARD" and current is not None: contacts.append(current); current = None
        elif current is not None and ":" in line:
            field, value = line.split(":", 1); kind = field.split(";", 1)[0].upper()
            if kind == "FN": current["name"] = value
            elif kind == "TEL":
                phones = current["phones"]
                if isinstance(phones, list): phones.append(value)
        if len(contacts) >= 10_000: raise ValueError("contact limit exceeded")
    if current is not None: raise ValueError("unterminated vCard")
    return contacts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("file", type=Path); args = parser.parse_args(argv)
    try:
        values = parse_contacts(read_bytes(args.file).decode("utf-8"))
        for contact in values:
            print(contact["name"])
            for phone in contact["phones"]: print(f"  {phone}")
        print(f"Total contacts: {len(values)}")
        return 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"contacts: {error}", file=__import__("sys").stderr); return 1


if __name__ == "__main__": raise SystemExit(main())

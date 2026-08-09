"""Extract names and telephone numbers from a vCard file."""

import argparse
from pathlib import Path
from typing import Dict, List, Optional


def parse_contacts(text: str) -> List[Dict[str, object]]:
    contacts: List[Dict[str, object]] = []
    current: Optional[Dict[str, object]] = None
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line.upper() == "BEGIN:VCARD":
            current = {"name": "(unnamed)", "phones": []}
        elif line.upper() == "END:VCARD" and current is not None:
            contacts.append(current)
            current = None
        elif current is not None and ":" in line:
            field, value = line.split(":", 1)
            kind = field.split(";", 1)[0].upper()
            if kind == "FN":
                current["name"] = value
            elif kind == "TEL":
                phones = current["phones"]
                assert isinstance(phones, list)
                phones.append(value)
    return contacts


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="vCard (.vcf) file")
    args = parser.parse_args()
    contacts = parse_contacts(args.file.read_text(encoding="utf-8"))
    for contact in contacts:
        print(contact["name"])
        for phone in contact["phones"]:
            print(f"  {phone}")
    print(f"Total contacts: {len(contacts)}")


if __name__ == "__main__":
    main()

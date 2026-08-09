"""Extract links from an HTML file."""

import argparse
from html.parser import HTMLParser
from pathlib import Path
from typing import List, Optional, Tuple


class LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.links: List[str] = []

    def handle_starttag(
        self, tag: str, attrs: List[Tuple[str, Optional[str]]]
    ) -> None:
        if tag.casefold() == "a":
            href = dict(attrs).get("href")
            if href is not None:
                self.links.append(href)


def extract_links(html: str) -> List[str]:
    parser = LinkParser()
    parser.feed(html)
    return parser.links


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="HTML file to parse")
    args = parser.parse_args()
    print(extract_links(args.file.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()

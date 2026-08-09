"""Store and list question-and-answer categories in SQLite."""

import argparse
import sqlite3
from pathlib import Path
from typing import List, Tuple


def initialize_database(path: Path) -> None:
    with sqlite3.connect(str(path)) as connection:
        connection.execute(
            """CREATE TABLE IF NOT EXISTS category (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                answer TEXT NOT NULL
            )"""
        )


def add_category(path: Path, name: str, answer: str) -> None:
    initialize_database(path)
    with sqlite3.connect(str(path)) as connection:
        connection.execute(
            """INSERT INTO category (name, answer) VALUES (?, ?)
               ON CONFLICT(name) DO UPDATE SET answer = excluded.answer""",
            (name, answer),
        )


def list_categories(path: Path) -> List[Tuple[int, str, str]]:
    initialize_database(path)
    with sqlite3.connect(str(path)) as connection:
        return connection.execute(
            "SELECT id, name, answer FROM category ORDER BY name COLLATE NOCASE"
        ).fetchall()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=Path("jeo.db"))
    commands = parser.add_subparsers(dest="command", required=True)
    add = commands.add_parser("add", help="add or update an entry")
    add.add_argument("name")
    add.add_argument("answer")
    commands.add_parser("list", help="list entries")
    args = parser.parse_args()
    if args.command == "add":
        add_category(args.database, args.name, args.answer)
        print(f"Saved: {args.name}")
    else:
        for row_id, name, answer in list_categories(args.database):
            print(f"{row_id}: {name} — {answer}")


if __name__ == "__main__":
    main()

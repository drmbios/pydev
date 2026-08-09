"""Regression tests for the reusable application logic."""

import json
import random
import sqlite3
import tempfile
import unittest
from pathlib import Path

import cntr
import codebreaker
import parser_html
import rdr2dot0
import readjson
import sql
import test as contacts
import ttt
import write2file


class ApplicationTests(unittest.TestCase):
    def test_letter_counter(self):
        counts = cntr.count_letters("Abracadabra!")
        self.assertEqual(counts["A"], 1)
        self.assertEqual(counts["a"], 4)
        self.assertEqual(counts["z"], 0)

    def test_codebreaker(self):
        code = codebreaker.generate_code(random.Random(1))
        self.assertEqual(len(code), 4)
        self.assertEqual(len(set(code)), 4)
        self.assertEqual(codebreaker.score_guess("1234", "1243"), (2, 2))
        self.assertTrue(codebreaker.valid_guess("0123"))
        self.assertFalse(codebreaker.valid_guess("123"))

    def test_html_links(self):
        html = '<a href="one">One</a><A class="x" HREF="two">Two</A>'
        self.assertEqual(parser_html.extract_links(html), ["one", "two"])

    def test_tic_tac_toe(self):
        board = ["X", "X", "X", "", "O", "", "O", "", ""]
        self.assertEqual(ttt.winner(board), "X")
        with self.assertRaises(ValueError):
            ttt.make_move(board, 1, "O")

    def test_contacts(self):
        text = "BEGIN:VCARD\nFN:Alice\nTEL;TYPE=CELL:123\nEND:VCARD\n"
        self.assertEqual(
            contacts.parse_contacts(text),
            [{"name": "Alice", "phones": ["123"]}],
        )

    def test_files_json_and_database(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            json_path = root / "data.json"
            json_path.write_text(json.dumps({"a": 1}), encoding="utf-8")
            self.assertEqual(readjson.read_json(json_path), {"a": 1})
            self.assertEqual(rdr2dot0.read_json(json_path), {"a": 1})

            text_path = root / "note.txt"
            write2file.write_text(text_path, "hello")
            self.assertEqual(text_path.read_text(encoding="utf-8"), "hello")

            database = root / "answers.db"
            sql.add_category(database, "Greeting", "Hello")
            sql.add_category(database, "Greeting", "Hi")
            self.assertEqual(sql.list_categories(database), [(1, "Greeting", "Hi")])
            with sqlite3.connect(str(database)) as connection:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM category").fetchone()[0], 1)


if __name__ == "__main__":
    unittest.main()

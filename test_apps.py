"""Regression tests for the reusable application logic."""

import json
import os
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
import contacts
import ttt
import write2file
import antivermis
import checksum
import clockres
import coreinfo
import hexview
import linkscan
import lsx
import numconv
import randpass
import stringsx
import syscallx


class ApplicationTests(unittest.TestCase):
    def test_new_python_utility_logic(self):
        self.assertEqual(numconv.convert("0xff")["decimal"], "255")
        self.assertEqual(stringsx.strings(b"ab\0hello\xffworld", 5), ["hello", "world"])
        self.assertIn("00000000", hexview.format_hex(b"ABC"))
        self.assertEqual(len(randpass.generate(32)), 32)
        self.assertEqual(syscallx.load_table()[2][0], "open")
        self.assertGreater(clockres.resolutions()["monotonic"], 0)
        self.assertIn("architecture", coreinfo.information())

    def test_python_file_tools_and_antivermis(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            vector = root / "vector"
            vector.write_bytes(b"abc")
            self.assertEqual(checksum.crc32_file(str(vector)), 0x352441C2)
            self.assertEqual(lsx.real_size(vector), 3)
            alias = root / "alias"
            os.link(vector, alias)
            self.assertEqual(set(linkscan.find_links(vector, root)), {vector, alias})

            database = b"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad:3:python-test\n"
            signatures = antivermis.parse_database(database)
            result = antivermis.scan([vector], signatures)
            self.assertEqual(result.findings[0][0], "AV-SIG-001")

            eicar = root / "eicar.com"
            eicar.write_bytes(b"X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*")
            self.assertEqual(antivermis.scan([eicar], {}).findings[0][0], "AV-TEST-001")

            source = root / "source.hsb"
            source.write_bytes(database)
            digest = __import__("hashlib").sha256(database).hexdigest()
            manifest = root / "manifest"
            manifest.write_text(
                "ANTIVERMIS-MANIFEST 1\nversion 1.0.0\n"
                f"database {source.as_uri()}\nsha256 {digest}\n",
                encoding="ascii",
            )
            installed = root / "installed.hsb"
            self.assertIn("updated", antivermis.update_database(manifest.as_uri(), installed))
            self.assertEqual(installed.read_bytes(), database)
            self.assertIn("current", antivermis.update_database(manifest.as_uri(), installed, True))

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
            link_path = root / "note-link.txt"
            link_path.symlink_to(text_path)
            with self.assertRaises(OSError):
                write2file.write_text(link_path, "replacement")
            self.assertEqual(text_path.read_text(encoding="utf-8"), "hello")

            database = root / "answers.db"
            sql.add_category(database, "Greeting", "Hello")
            sql.add_category(database, "Greeting", "Hi")
            self.assertEqual(sql.list_categories(database), [(1, "Greeting", "Hi")])
            with sqlite3.connect(str(database)) as connection:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM category").fetchone()[0], 1)


if __name__ == "__main__":
    unittest.main()

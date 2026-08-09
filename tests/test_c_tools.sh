#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pydev-c.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

printf 'Abracadabra!\n' > "$tmp/text.txt"
"$root/bin/cntr" "$tmp/text.txt" | grep -q '^a 5$'

"$root/bin/write2file" "$tmp/output.txt" 'safe text' >/dev/null
test "$(cat "$tmp/output.txt")" = 'safe text'

printf '<a href="one">1</a><A HREF='"'"'two'"'"'>2</A>\n' > "$tmp/page.html"
test "$("$root/bin/parser_html" "$tmp/page.html")" = "one
two"

printf '{"name":"demo","items":[1,2]}\n' > "$tmp/data.json"
"$root/bin/readjson" "$tmp/data.json" | grep -q '"items": '
"$root/bin/rdr2dot0" "$tmp/data.json" | grep -q '"demo"'

printf 'BEGIN:VCARD\nFN:Alice\nTEL;TYPE=CELL:123\nEND:VCARD\n' > "$tmp/contacts.vcf"
"$root/bin/contacts" "$tmp/contacts.vcf" | grep -q 'Total contacts: 1; phone numbers: 1'

"$root/bin/sql" "$tmp/app.db" add Greeting Hello >/dev/null
"$root/bin/sql" "$tmp/app.db" add Greeting Hi >/dev/null
"$root/bin/sql" "$tmp/app.db" list | grep -q 'Greeting - Hi'

printf '1\n4\n2\n5\n3\n' | "$root/bin/ttt" | grep -q 'Player X wins!'
printf '0000\n1111\n' | "$root/bin/codebreaker" 2 >/dev/null || test $? -eq 1

# Oversized inputs must be rejected before allocation or parsing.
dd if=/dev/zero of="$tmp/oversized" bs=1048576 count=17 2>/dev/null
if "$root/bin/cntr" "$tmp/oversized" >/dev/null 2>&1; then
    echo "oversized input was accepted" >&2
    exit 1
fi

# Invalid package actions are rejected and never passed through a shell.
if "$root/bin/qpipper" run 'x;touch /tmp/should-not-exist' >/dev/null 2>&1; then
    echo "invalid package action was accepted" >&2
    exit 1
fi

echo "C tool tests passed"

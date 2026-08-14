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
printf 'href="text"<!-- <a href="comment"> --><div data-href="wrong" href="right"></div><a href>bad</a>\n' > "$tmp/tricky.html"
test "$("$root/bin/parser_html" "$tmp/tricky.html" 2>/dev/null)" = 'right'

printf '{"name":"demo","items":[1,2]}\n' > "$tmp/data.json"
"$root/bin/readjson" "$tmp/data.json" | grep -q '"items": '
"$root/bin/rdr2dot0" "$tmp/data.json" | grep -q '"demo"'
for invalid_json in '{foo}' '[1,]' '01' '"bad\q"' '{}{}'; do
    printf '%s' "$invalid_json" > "$tmp/invalid.json"
    if "$root/bin/readjson" "$tmp/invalid.json" >/dev/null 2>&1; then
        echo "readjson accepted invalid JSON: $invalid_json" >&2
        exit 1
    fi
done
printf '{}' > "$tmp/empty-object.json"
test "$("$root/bin/readjson" "$tmp/empty-object.json")" = '{}'

printf 'BEGIN:VCARD\nFN:Alice\nTEL;TYPE=CELL:123\nEND:VCARD\n' > "$tmp/contacts.vcf"
"$root/bin/contacts" "$tmp/contacts.vcf" | grep -q 'Total contacts: 1; phone numbers: 1'

if "$root/bin/sql" --backend | grep -q '^sqlite3 '; then
    "$root/bin/sql" "$tmp/app.db" add Greeting Hello >/dev/null
    "$root/bin/sql" "$tmp/app.db" add Greeting Hi >/dev/null
    "$root/bin/sql" "$tmp/app.db" list | grep -q 'Greeting - Hi'
else
    "$root/bin/sql" --backend | grep -q '^unavailable'
    if "$root/bin/sql" "$tmp/app.db" list >/dev/null 2>&1; then
        echo "sql fallback unexpectedly accepted a database operation" >&2
        exit 1
    fi
fi

printf '1\n4\n2\n5\n3\n' | "$root/bin/ttt" | grep -q 'Player X wins!'
{ printf '%080d\n' 0; printf '1\n4\n2\n5\n3\n'; } | "$root/bin/ttt" | grep -q 'Player X wins!'
printf '0000\n1111\n' | "$root/bin/codebreaker" 2 >/dev/null || test $? -eq 1

printf '123456789' > "$tmp/checksum.txt"
"$root/bin/checksum" "$tmp/checksum.txt" | grep -q '^cbf43926  '

printf 'ABC\000xyz!\n' > "$tmp/binary.dat"
"$root/bin/hexview" "$tmp/binary.dat" | grep -q '41 42 43 00 78 79 7a 21'
test "$("$root/bin/stringsx" "$tmp/binary.dat" 4)" = 'xyz!'

password=$("$root/bin/randpass" 64)
test "${#password}" -eq 64
if "$root/bin/randpass" 7 >/dev/null 2>&1; then
    echo "randpass accepted an unsafe length" >&2
    exit 1
fi

"$root/bin/syscallx" 2 | grep -q 'meaning: Open a file'
printf '#define __NR_openat2 437\n' > "$tmp/unistd_64.h"
"$root/bin/syscallx" --table "$tmp/unistd_64.h" 437 | grep -q 'name: openat2'
if "$root/bin/syscallx" nope >/dev/null 2>&1; then
    echo "syscallx accepted a non-numeric syscall" >&2
    exit 1
fi

mkdir -p "$tmp/listing/sub"
dd if=/dev/zero of="$tmp/listing/one.bin" bs=1024 count=1 2>/dev/null
dd if=/dev/zero of="$tmp/listing/sub/large.bin" bs=1048576 count=1 2>/dev/null
printf 'hidden' > "$tmp/listing/.hidden"
chmod 700 "$tmp/listing/one.bin"
ln -s . "$tmp/listing/self-link"
"$root/bin/lsx" "$tmp/listing" | grep -Eq '^700 +FILE +1\.00 KB +one\.bin$'
"$root/bin/lsx" "$tmp/listing" | grep -Eq '^755 +DIR +1\.00 MB +sub$'
"$root/bin/lsx" -a "$tmp/listing" | grep -q '\.hidden$'
"$root/bin/lsx" "$tmp/listing" | grep -Eq 'LINK +1 B +self-link$'
"$root/bin/lsx" -S "$tmp/listing" | awk 'NR == 3 { if ($NF != "sub") exit 1 }'

if test "$(uname -s)" = Linux; then
    # traceflow launches its own harmless child; it never attaches to an existing PID.
    "$root/bin/traceflow" /bin/true > "$tmp/traceflow.txt"
    grep -q 'exec' "$tmp/traceflow.txt"
    grep -q 'exited=0' "$tmp/traceflow.txt"

    "$root/bin/syswatch" --interval 1 --count 1 --log "$tmp/metrics" > "$tmp/syswatch.txt"
    grep -q 'CPU' "$tmp/syswatch.txt"
    for metric in cpu memory network disk load; do
        test -s "$tmp/metrics/$metric.csv"
    done
    printf 'Linux demo\n12:00:00 CPU %%user\n\001unsafe\n' > "$tmp/sar.txt"
    "$root/bin/syswatch" --sar "$tmp/sar.txt" | grep -q '?unsafe'
    ln -s "$tmp/sar.txt" "$tmp/sar-link"
    if "$root/bin/syswatch" --sar "$tmp/sar-link" >/dev/null 2>&1; then
        echo "syswatch followed a SAR symlink" >&2
        exit 1
    fi
    ln -s "$tmp/metrics" "$tmp/metrics-link"
    if "$root/bin/syswatch" --interval 1 --count 1 --log "$tmp/metrics-link" >/dev/null 2>&1; then
        echo "syswatch accepted a symlink log directory" >&2
        exit 1
    fi

    "$root/bin/procexp" 1 | grep -q 'environment values are intentionally hidden'
    if "$root/bin/procexp" 2147483647 >/dev/null 2>&1; then
        echo "procexp accepted an unavailable PID" >&2
        exit 1
    fi
    if "$root/bin/sessionx" --terminate root >/dev/null 2>"$tmp/sessionx.err"; then
        echo "sessionx accepted root termination" >&2
        exit 1
    fi
    grep -q 'refusing to terminate root' "$tmp/sessionx.err"
fi

# Oversized inputs must be rejected before allocation or parsing.
dd if=/dev/zero of="$tmp/oversized" bs=1048576 count=17 2>/dev/null
if "$root/bin/cntr" "$tmp/oversized" >/dev/null 2>&1; then
    echo "oversized input was accepted" >&2
    exit 1
fi
if "$root/bin/hexview" "$tmp/oversized" >/dev/null 2>&1; then
    echo "hexview accepted oversized input" >&2
    exit 1
fi

# Invalid package actions are rejected and never passed through a shell.
if "$root/bin/qpipper" run 'x;touch /tmp/should-not-exist' >/dev/null 2>&1; then
    echo "invalid package action was accepted" >&2
    exit 1
fi
if "$root/bin/qpipper" install '--target=/tmp/injected' >/dev/null 2>&1; then
    echo "pip option injection was accepted" >&2
    exit 1
fi

mkfifo "$tmp/input.fifo"
if "$root/bin/cntr" "$tmp/input.fifo" >/dev/null 2>&1; then
    echo "FIFO input was accepted" >&2
    exit 1
fi

printf 'original' > "$tmp/target.txt"
ln -s "$tmp/target.txt" "$tmp/write-link"
if "$root/bin/write2file" "$tmp/write-link" replacement >/dev/null 2>&1; then
    echo "write2file followed a symbolic link" >&2
    exit 1
fi
test "$(cat "$tmp/target.txt")" = original

# Random binary input must fail JSON validation without crashing other bounded readers.
dd if=/dev/urandom of="$tmp/random.bin" bs=65536 count=1 2>/dev/null
if "$root/bin/readjson" "$tmp/random.bin" >/dev/null 2>&1; then
    echo "readjson unexpectedly accepted random binary input" >&2
    exit 1
fi
"$root/bin/parser_html" "$tmp/random.bin" >/dev/null 2>&1
"$root/bin/hexview" "$tmp/random.bin" >/dev/null
"$root/bin/stringsx" "$tmp/random.bin" >/dev/null

echo "C tool tests passed"

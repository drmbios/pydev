"""Bounded WHOIS client with validated inputs and timeouts."""

from __future__ import annotations

import argparse
import re
import socket

VALID = re.compile(r"[A-Za-z0-9][A-Za-z0-9.:-]{0,252}\Z")


def query(name: str, server: str = "whois.iana.org", limit: int = 1024 * 1024) -> str:
    if not VALID.fullmatch(name) or not VALID.fullmatch(server):
        raise ValueError("invalid WHOIS name or server")
    chunks, total = [], 0
    with socket.create_connection((server, 43), timeout=10) as connection:
        connection.settimeout(10); connection.sendall((name + "\r\n").encode("ascii"))
        while True:
            chunk = connection.recv(min(8192, limit + 1 - total))
            if not chunk: break
            total += len(chunk)
            if total > limit: raise ValueError("WHOIS response exceeds limit")
            chunks.append(chunk)
    return b"".join(chunks).decode("utf-8", "replace")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--server", default="whois.iana.org"); parser.add_argument("name")
    args = parser.parse_args(argv)
    try: print(query(args.name, args.server), end=""); return 0
    except (OSError, ValueError) as error: print(f"whoisx: {error}", file=__import__("sys").stderr); return 1


if __name__ == "__main__": raise SystemExit(main())

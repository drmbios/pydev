"""Read-only Linux process explorer; environment values are never displayed."""

from __future__ import annotations

import os
import pwd
import sys
from pathlib import Path

PROC = Path("/proc")
MAX_PROCESSES = 100_000


def process_info(pid: int) -> dict[str, str | int]:
    root = PROC / str(pid)
    status = {}
    for line in (root / "status").read_text(errors="replace").splitlines()[:256]:
        if ":" in line:
            key, value = line.split(":", 1); status[key] = value.strip()
    uid = int(status.get("Uid", "-1").split()[0])
    try: owner = pwd.getpwuid(uid).pw_name
    except KeyError: owner = str(uid)
    try: executable = os.readlink(root / "exe")
    except OSError: executable = "unavailable"
    try: descriptors = sum(1 for _ in os.scandir(root / "fd"))
    except OSError: descriptors = -1
    return {"pid": pid, "name": status.get("Name", "?"), "owner": owner,
            "state": status.get("State", "?"), "memory": status.get("VmRSS", "?"),
            "threads": int(status.get("Threads", "0")), "descriptors": descriptors,
            "executable": executable}


def processes() -> list[dict[str, str | int]]:
    if not PROC.is_dir():
        raise RuntimeError("process inventory requires Linux /proc")
    result = []
    for entry in os.scandir(PROC):
        if entry.name.isdigit():
            try: result.append(process_info(int(entry.name)))
            except (OSError, ValueError): pass
            if len(result) >= MAX_PROCESSES: break
    return result


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    try:
        items = [process_info(int(args[0]))] if len(args) == 1 else processes() if not args else (_ for _ in ()).throw(ValueError())
        for item in items:
            print(" ".join(f"{key}={value}" for key, value in item.items()))
        if len(args) == 1: print("environment values are intentionally hidden")
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"procexp: {error}", file=sys.stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

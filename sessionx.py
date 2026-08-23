"""List login sessions and cautiously terminate a user's processes on Linux."""

from __future__ import annotations

import argparse
import os
import pwd
import signal
import subprocess
import sys


def user_processes(username: str) -> list[int]:
    uid = pwd.getpwnam(username).pw_uid
    pids = []
    for name in os.listdir("/proc"):
        if not name.isdigit(): continue
        try:
            if os.stat(f"/proc/{name}").st_uid == uid: pids.append(int(name))
        except OSError: pass
    return pids[:100_000]


def sessions() -> str:
    try: return subprocess.run(["who"], check=False, capture_output=True, text=True, timeout=5).stdout
    except (OSError, subprocess.TimeoutExpired): return ""


def terminate(username: str, confirmed: bool = False) -> int:
    account = pwd.getpwnam(username)
    if account.pw_uid == 0 or username == "root": raise PermissionError("refusing to terminate root")
    targets = [pid for pid in user_processes(username) if pid not in (1, os.getpid(), os.getppid())]
    if not confirmed:
        print(f"dry run: would send SIGTERM to {len(targets)} process(es) owned by {username}")
        return 0
    for pid in targets:
        try:
            if os.stat(f"/proc/{pid}").st_uid == account.pw_uid: os.kill(pid, signal.SIGTERM)
        except (FileNotFoundError, ProcessLookupError): pass
    print(f"sent SIGTERM to {len(targets)} process(es) owned by {username}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--terminate"); parser.add_argument("--confirm", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.confirm and not args.terminate: parser.error("--confirm requires --terminate")
        if args.terminate: return terminate(args.terminate, args.confirm)
        print(sessions(), end=""); return 0
    except (KeyError, OSError, PermissionError) as error:
        print(f"sessionx: {error}", file=sys.stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

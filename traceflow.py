"""Launch a command under a bounded syscall tracer and render process events."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def trace(command: list[str]) -> tuple[int, str]:
    tracer = shutil.which("strace")
    if not tracer: raise RuntimeError("traceflow requires strace on Linux")
    with tempfile.TemporaryDirectory(prefix="traceflow-") as directory:
        prefix = str(Path(directory) / "trace")
        result = subprocess.run([tracer, "-ff", "-qq", "-o", prefix, "--", *command], check=False, timeout=60)
        lines, count = [], 0
        for path in sorted(Path(directory).glob("trace*")):
            lines.append(f"process {path.suffix.removeprefix('.') or '?'}")
            for line in path.read_text(errors="replace").splitlines():
                count += 1
                if count > 100_000: raise RuntimeError("trace event limit exceeded")
                lines.append(f"  {line[:4096]}")
        return result.returncode, "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if not args: print("usage: traceflow.py COMMAND [ARG...]", file=sys.stderr); return 2
    try:
        code, output = trace(args); print(output); return code
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"traceflow: {error}", file=sys.stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

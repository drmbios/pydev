"""A small wrapper around the current Python interpreter's pip command."""

import argparse
import subprocess
import sys


def run_pip(action: str, package: str) -> int:
    command = [sys.executable, "-m", "pip"]
    if action == "search":
        command.extend(["index", "versions", package])
    else:
        command.extend(["install", package])
    return subprocess.run(command, check=False).returncode


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("search", "install"))
    parser.add_argument("package", help="package name or install specifier")
    args = parser.parse_args()
    raise SystemExit(run_pip(args.action, args.package))


if __name__ == "__main__":
    main()

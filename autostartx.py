"""Read-only inventory of standard Linux and macOS startup locations."""

from __future__ import annotations

import os
import platform
import argparse
import json
from pathlib import Path


def locations(home: Path | None = None) -> list[Path]:
    home = Path.home() if home is None else home
    if platform.system() == "Darwin":
        return [Path("/Library/LaunchAgents"), Path("/Library/LaunchDaemons"), home / "Library/LaunchAgents"]
    return [Path("/etc/systemd/system"), Path("/usr/lib/systemd/system"), Path("/etc/cron.d"), home / ".config/autostart", home / ".config/systemd/user"]


def inventory(home: Path | None = None) -> list[Path]:
    output = []
    for root in locations(home):
        try:
            output.extend(Path(entry.path) for entry in os.scandir(root) if not entry.is_symlink())
        except OSError:
            continue
    return output[:10_000]


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--json", action="store_true"); args = parser.parse_args(argv)
    if args.json:
        from pydev_ai.contract import ResponseBuilder
        builder = ResponseBuilder("list_startup_entries")
        entries = []
        for item in inventory():
            evidence_id = builder.add_evidence("startup_entry", {"listed": True, "content_inspected": False}, str(item))
            entries.append({"path": str(item), "listed": True, "content_inspected": False, "malicious": None, "evidence_id": evidence_id})
        builder.coverage.scanned = len(entries)
        print(json.dumps(builder.finish({"entries": entries, "interpretation": "listing does not prove an entry is active or malicious"}), sort_keys=True))
    else:
        for item in inventory(): print(item)
    return 0


if __name__ == "__main__": raise SystemExit(main())

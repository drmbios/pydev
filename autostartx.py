"""Read-only inventory of standard Linux and macOS startup locations."""

from __future__ import annotations

import os
import platform
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


if __name__ == "__main__":
    for item in inventory():
        print(item)

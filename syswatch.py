"""Bounded Linux CPU, memory, load, disk, and network monitor."""

from __future__ import annotations

import argparse
import csv
import os
import stat
import time
from pathlib import Path
from pycommon import read_bytes, safe_text


def snapshot() -> dict[str, float]:
    if not Path("/proc/stat").exists(): raise RuntimeError("syswatch requires Linux /proc")
    cpu = [int(value) for value in Path("/proc/stat").read_text().splitlines()[0].split()[1:9]]
    memory = {}
    for line in Path("/proc/meminfo").read_text().splitlines():
        key, value = line.split(":", 1); memory[key] = int(value.split()[0])
    network = 0
    for line in Path("/proc/net/dev").read_text().splitlines()[2:]:
        fields = line.replace(":", " ").split(); network += int(fields[1]) + int(fields[9])
    disk = os.statvfs("/")
    return {"cpu_total": float(sum(cpu)), "cpu_idle": float(cpu[3] + cpu[4]),
            "memory_used_kb": float(memory.get("MemTotal", 0) - memory.get("MemAvailable", 0)),
            "load": os.getloadavg()[0], "network_bytes": float(network),
            "disk_used_bytes": float((disk.f_blocks - disk.f_bfree) * disk.f_frsize)}


def show_sar(path: str) -> int:
    for line in read_bytes(path).decode("utf-8", "replace").splitlines(): print(safe_text(line))
    return 0


def log_metrics(directory: Path, values: dict[str, float]) -> None:
    if directory.is_symlink(): raise ValueError("log directory must not be a symlink")
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    for name, value in (("cpu", values["cpu_total"]), ("memory", values["memory_used_kb"]),
                        ("network", values["network_bytes"]), ("disk", values["disk_used_bytes"]), ("load", values["load"])):
        path = directory / f"{name}.csv"
        if path.is_symlink(): raise ValueError("log file must not be a symlink")
        descriptor = os.open(path, os.O_WRONLY | os.O_APPEND | os.O_CREAT | getattr(os, "O_NOFOLLOW", 0), 0o600)
        with os.fdopen(descriptor, "a", newline="") as stream: csv.writer(stream).writerow((int(time.time()), value))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--interval", type=int, default=1); parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--log", type=Path); parser.add_argument("--sar")
    args = parser.parse_args(argv)
    try:
        if args.sar: return show_sar(args.sar)
        if not 1 <= args.interval <= 3600 or not 1 <= args.count <= 1_000_000: raise ValueError("invalid interval or count")
        for index in range(args.count):
            values = snapshot(); print(" ".join(f"{key}={value:.0f}" for key, value in values.items()))
            if args.log: log_metrics(args.log, values)
            if index + 1 < args.count: time.sleep(args.interval)
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"syswatch: {error}", file=__import__("sys").stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

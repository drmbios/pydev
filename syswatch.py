"""Bounded Linux CPU, memory, load, disk, and network monitor."""

from __future__ import annotations

import argparse
import csv
import json
import os
import time
from pathlib import Path
from pycommon import read_bytes, safe_text


def snapshot(interval: float = 1.0) -> dict:
    from pydev_ai.metrics import sample_system
    return sample_system(interval)


def show_sar(path: str) -> int:
    for line in read_bytes(path).decode("utf-8", "replace").splitlines(): print(safe_text(line))
    return 0


def log_metrics(directory: Path, values: dict) -> None:
    if directory.is_symlink(): raise ValueError("log directory must not be a symlink")
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    pairs = (("cpu", values["cpu"]["utilization_percent"]), ("memory", values["memory"]["used_bytes"]),
             ("network", values["network_aggregate"]["rx_bytes_per_second"] + values["network_aggregate"]["tx_bytes_per_second"]),
             ("disk", sum(row["read_bytes_per_second"] + row["write_bytes_per_second"] for row in values["disk_devices"])),
             ("load", os.getloadavg()[0]))
    for name, value in pairs:
        path = directory / f"{name}.csv"
        if path.is_symlink(): raise ValueError("log file must not be a symlink")
        descriptor = os.open(path, os.O_WRONLY | os.O_APPEND | os.O_CREAT | getattr(os, "O_NOFOLLOW", 0), 0o600)
        with os.fdopen(descriptor, "a", newline="") as stream: csv.writer(stream).writerow((int(time.time()), value))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--interval", type=int, default=1); parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--log", type=Path); parser.add_argument("--sar"); parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.sar: return show_sar(args.sar)
        if not 1 <= args.interval <= 3600 or not 1 <= args.count <= 1_000_000: raise ValueError("invalid interval or count")
        for index in range(args.count):
            values = snapshot(float(args.interval))
            if args.json:
                from pydev_ai.contract import ResponseBuilder
                builder = ResponseBuilder("sample_system_metrics")
                builder.warnings.extend(values.pop("warnings"))
                evidence_id = builder.add_evidence("sample_window", {"duration_seconds": values["sample_duration_seconds"]}, "monotonic clock")
                values["evidence_id"] = evidence_id
                print(json.dumps(builder.finish(values), sort_keys=True))
            else:
                print("CPU={:.2f}% Memory={:.2f}% Network_RX={:.0f}B/s Network_TX={:.0f}B/s".format(
                    values["cpu"]["utilization_percent"], values["memory"]["utilization_percent"],
                    values["network_aggregate"]["rx_bytes_per_second"], values["network_aggregate"]["tx_bytes_per_second"]))
            if args.log: log_metrics(args.log, values)
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"syswatch: {error}", file=__import__("sys").stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

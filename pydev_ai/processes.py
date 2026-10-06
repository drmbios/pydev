"""Linux process identity, metadata, and sampled CPU measurements."""

from __future__ import annotations

import os
import pwd
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


@dataclass(frozen=True)
class ProcessCounter:
    pid: int
    start_ticks: int
    cpu_ticks: int


def parse_proc_stat(pid: int, text: str) -> ProcessCounter:
    end = text.rfind(")")
    if end < 0: raise ValueError("malformed process stat")
    fields = text[end + 2:].split()
    if len(fields) < 20: raise ValueError("short process stat")
    return ProcessCounter(pid, int(fields[19]), int(fields[11]) + int(fields[12]))


def read_status(path: Path) -> Dict[str, str]:
    values: Dict[str, str] = {}
    for line in path.read_text(errors="replace").splitlines()[:256]:
        key, separator, value = line.partition(":")
        if separator: values[key] = value.strip()
    return values


def process_metadata(pid: int, proc: Path = Path("/proc")) -> dict:
    root = proc / str(pid)
    status = read_status(root / "status")
    counter = parse_proc_stat(pid, (root / "stat").read_text())
    uid = int(status.get("Uid", "-1").split()[0])
    try: owner = pwd.getpwuid(uid).pw_name
    except KeyError: owner = str(uid)
    warnings: List[str] = []
    try: executable: Optional[str] = os.readlink(root / "exe")
    except PermissionError: executable = None; warnings.append("executable path permission denied")
    except OSError: executable = None; warnings.append("executable path unavailable")
    try: descriptors: Optional[int] = sum(1 for _ in os.scandir(root / "fd"))
    except PermissionError: descriptors = None; warnings.append("descriptor count permission denied")
    except OSError: descriptors = None; warnings.append("descriptor count unavailable")
    memory_kib = int(status.get("VmRSS", "0 kB").split()[0])
    return {"pid": pid, "start_identity_ticks": counter.start_ticks, "name": status.get("Name", "?"),
            "owner": owner, "uid": uid, "state": status.get("State", "?"),
            "resident_memory_bytes": memory_kib * 1024, "threads": int(status.get("Threads", "0")),
            "descriptor_count": descriptors, "executable": executable, "warnings": warnings}


def read_counters(proc: Path = Path("/proc"), max_processes: int = 4096) -> tuple[Dict[int, ProcessCounter], int, int]:
    counters: Dict[int, ProcessCounter] = {}; skipped = failed = 0
    try: entries = os.scandir(proc)
    except OSError as error: raise NotImplementedError("process collection requires readable Linux /proc") from error
    with entries:
        for entry in entries:
            if not entry.name.isdigit(): continue
            if len(counters) >= max_processes: skipped += 1; continue
            pid = int(entry.name)
            try: counters[pid] = parse_proc_stat(pid, (Path(entry.path) / "stat").read_text())
            except (OSError, ValueError): failed += 1
    return counters, skipped, failed


def process_cpu_rates(before: Dict[int, ProcessCounter], after: Dict[int, ProcessCounter],
                      elapsed: float, clock_ticks: int) -> tuple[Dict[int, float], int, int, int]:
    if elapsed <= 0 or clock_ticks <= 0: raise ValueError("invalid process sample timing")
    rates: Dict[int, float] = {}; reused = reset = 0
    for pid, new in after.items():
        old = before.get(pid)
        if old is None: continue
        if old.start_ticks != new.start_ticks: reused += 1; continue
        if new.cpu_ticks < old.cpu_ticks: reset += 1; continue
        rates[pid] = 100.0 * ((new.cpu_ticks - old.cpu_ticks) / clock_ticks) / elapsed
    return rates, len(set(before) - set(after)), reused, reset


def sample_processes(interval: float = 1.0, max_processes: int = 4096,
                     proc: Path = Path("/proc"), sleeper=time.sleep,
                     clock_ticks: Optional[int] = None) -> dict:
    if not 0.1 <= interval <= 30.0: raise ValueError("sample interval must be 0.1-30 seconds")
    hz = clock_ticks or int(os.sysconf("SC_CLK_TCK"))
    before, skipped_before, failed_before = read_counters(proc, max_processes)
    start = time.monotonic(); sleeper(interval)
    after, skipped_after, failed_after = read_counters(proc, max_processes)
    elapsed = time.monotonic() - start
    rates, disappeared, reused, reset = process_cpu_rates(before, after, elapsed, hz)
    rows, metadata_failed = [], 0
    for pid, rate in rates.items():
        try: metadata = process_metadata(pid, proc)
        except (OSError, ValueError): metadata_failed += 1; continue
        if metadata["start_identity_ticks"] != after[pid].start_ticks:
            reused += 1
            continue
        metadata["cpu_percent_one_core"] = rate
        rows.append(metadata)
    disappeared = len(set(before) - set(after))
    rows.sort(key=lambda item: (-item["cpu_percent_one_core"], -item["resident_memory_bytes"], item["pid"]))
    return {"sample_duration_seconds": elapsed, "normalization": "100% equals one fully utilized logical CPU core",
            "processes": rows, "disappeared": disappeared, "pid_reuse_excluded": reused,
            "counter_resets_excluded": reset,
            "skipped": skipped_before + skipped_after, "failed": failed_before + failed_after + metadata_failed}

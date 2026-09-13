"""Deterministic Linux counter parsers and rate calculations."""

from __future__ import annotations

import os
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


@dataclass(frozen=True)
class CpuCounters:
    user: int
    nice: int
    system: int
    idle: int
    iowait: int
    irq: int
    softirq: int
    steal: int
    guest: int
    guest_nice: int

    @property
    def total(self) -> int:
        # Linux user/nice already include guest time, so guest fields are not added.
        return self.user + self.nice + self.system + self.idle + self.iowait + self.irq + self.softirq + self.steal


def parse_cpu_line(line: str) -> CpuCounters:
    fields = line.split()
    if not fields or fields[0] != "cpu":
        raise ValueError("missing aggregate cpu counters")
    values = [int(value) for value in fields[1:11]]
    values.extend([0] * (10 - len(values)))
    if any(value < 0 for value in values):
        raise ValueError("negative cpu counter")
    return CpuCounters(*values[:10])


def cpu_utilization(before: CpuCounters, after: CpuCounters) -> Dict[str, float]:
    old = before.__dict__; new = after.__dict__
    if any(new[name] < old[name] for name in old):
        raise ValueError("cpu counter reset")
    delta_total = after.total - before.total
    if delta_total <= 0:
        raise ValueError("cpu counters did not advance")
    delta_idle = (after.idle - before.idle) + (after.iowait - before.iowait)
    delta_iowait = after.iowait - before.iowait
    busy = max(0, delta_total - delta_idle)
    return {
        "utilization_percent": 100.0 * busy / delta_total,
        "idle_percent": 100.0 * (after.idle - before.idle) / delta_total,
        "iowait_percent": 100.0 * delta_iowait / delta_total,
        "steal_percent": 100.0 * (after.steal - before.steal) / delta_total,
        "guest_delta_ticks": float((after.guest - before.guest) + (after.guest_nice - before.guest_nice)),
        "delta_ticks": float(delta_total),
    }


def parse_meminfo(text: str) -> Dict[str, int]:
    values: Dict[str, int] = {}
    for line in text.splitlines():
        key, separator, rest = line.partition(":")
        if separator and rest.split():
            amount = int(rest.split()[0])
            values[key] = amount * 1024
    total = values.get("MemTotal")
    available = values.get("MemAvailable")
    if total is None:
        raise ValueError("MemTotal unavailable")
    if available is None:
        available = values.get("MemFree", 0) + values.get("Buffers", 0) + values.get("Cached", 0)
    available = min(total, available)
    return {"total_bytes": total, "available_bytes": available, "used_bytes": total - available,
            "utilization_percent": 100.0 * (total - available) / total if total else 0.0}


def parse_netdev(text: str) -> Dict[str, Tuple[int, int]]:
    result: Dict[str, Tuple[int, int]] = {}
    for line in text.splitlines()[2:]:
        name, separator, counters = line.partition(":")
        fields = counters.split()
        if separator and len(fields) >= 16:
            result[name.strip()] = (int(fields[0]), int(fields[8]))
    return result


def network_rates(before: Dict[str, Tuple[int, int]], after: Dict[str, Tuple[int, int]], elapsed: float) -> tuple[List[dict], List[str]]:
    if elapsed <= 0:
        raise ValueError("non-positive sample interval")
    rows, warnings = [], []
    for name in sorted(set(before) | set(after)):
        if name not in before or name not in after:
            warnings.append("interface changed during sample: {}".format(name)); continue
        old_rx, old_tx = before[name]; new_rx, new_tx = after[name]
        if new_rx < old_rx or new_tx < old_tx:
            warnings.append("network counter reset: {}".format(name)); continue
        rows.append({"interface": name, "rx_bytes_per_second": (new_rx - old_rx) / elapsed,
                     "tx_bytes_per_second": (new_tx - old_tx) / elapsed})
    return rows, warnings


def parse_diskstats(text: str, allowed_devices: Optional[Iterable[str]] = None) -> Dict[str, Tuple[int, int]]:
    allowed = set(allowed_devices) if allowed_devices is not None else None
    result: Dict[str, Tuple[int, int]] = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) < 14: continue
        name = fields[2]
        if allowed is not None and name not in allowed: continue
        result[name] = (int(fields[5]) * 512, int(fields[9]) * 512)
    return result


def disk_rates(before: Dict[str, Tuple[int, int]], after: Dict[str, Tuple[int, int]], elapsed: float) -> tuple[List[dict], List[str]]:
    if elapsed <= 0: raise ValueError("non-positive sample interval")
    rows, warnings = [], []
    for name in sorted(set(before) | set(after)):
        if name not in before or name not in after:
            warnings.append("disk device changed during sample: {}".format(name)); continue
        old_read, old_write = before[name]; new_read, new_write = after[name]
        if new_read < old_read or new_write < old_write:
            warnings.append("disk counter reset: {}".format(name)); continue
        rows.append({"device": name, "read_bytes_per_second": (new_read - old_read) / elapsed,
                     "write_bytes_per_second": (new_write - old_write) / elapsed})
    return rows, warnings


def top_level_block_devices(sys_block: Path = Path("/sys/block")) -> tuple[List[str], Dict[str, List[str]]]:
    devices, stacked = [], {}
    try: entries = list(os.scandir(sys_block))
    except OSError: return devices, stacked
    for entry in entries:
        if entry.name.startswith(("loop", "ram", "zram")): continue
        slaves_dir = Path(entry.path) / "slaves"
        try: slaves = sorted(item.name for item in os.scandir(slaves_dir))
        except OSError: slaves = []
        stacked[entry.name] = slaves
        if not slaves: devices.append(entry.name)
    return sorted(devices), stacked


def disk_capacity(path: Path = Path("/")) -> Dict[str, float]:
    info = os.statvfs(path)
    total = info.f_blocks * info.f_frsize
    available = info.f_bavail * info.f_frsize
    used = total - info.f_bfree * info.f_frsize
    return {"mountpoint": str(path), "total_bytes": float(total), "used_bytes": float(used),
            "available_bytes": float(available), "utilization_percent": 100.0 * used / total if total else 0.0}


def read_linux_snapshot(proc: Path = Path("/proc")) -> dict:
    cpu = parse_cpu_line((proc / "stat").read_text().splitlines()[0])
    memory = parse_meminfo((proc / "meminfo").read_text())
    network = parse_netdev((proc / "net/dev").read_text())
    devices, stacked = top_level_block_devices()
    disks = parse_diskstats((proc / "diskstats").read_text(), devices)
    return {"cpu": cpu, "memory": memory, "network": network, "disks": disks,
            "stacked_devices": stacked, "monotonic": time.monotonic()}


def sample_system(interval: float, proc: Path = Path("/proc"), sleeper=time.sleep) -> dict:
    if not 0.1 <= interval <= 30.0: raise ValueError("sample interval must be 0.1-30 seconds")
    before = read_linux_snapshot(proc); sleeper(interval); after = read_linux_snapshot(proc)
    elapsed = after["monotonic"] - before["monotonic"]
    cpu = cpu_utilization(before["cpu"], after["cpu"])
    networks, network_warnings = network_rates(before["network"], after["network"], elapsed)
    disks, disk_warnings = disk_rates(before["disks"], after["disks"], elapsed)
    return {"sample_duration_seconds": elapsed, "cpu": cpu, "memory": after["memory"],
            "network_interfaces": networks, "network_aggregate": {
                "rx_bytes_per_second": sum(row["rx_bytes_per_second"] for row in networks),
                "tx_bytes_per_second": sum(row["tx_bytes_per_second"] for row in networks)},
            "disk_devices": disks, "disk_capacity": disk_capacity(),
            "disk_selection": {"aggregation": "leaf top-level /sys/block devices only", "stacked_devices": after["stacked_devices"]},
            "warnings": network_warnings + disk_warnings}

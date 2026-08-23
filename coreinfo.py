"""Cross-platform hardware and kernel summary."""

from __future__ import annotations

import os
import platform


def information() -> dict[str, str]:
    page = os.sysconf("SC_PAGE_SIZE") if hasattr(os, "sysconf") else 0
    memory = "unknown"
    try:
        memory = str(os.sysconf("SC_PHYS_PAGES") * page)
    except (ValueError, OSError):
        pass
    return {"system": platform.system(), "kernel": platform.release(), "architecture": platform.machine(),
            "cpu": platform.processor() or "unknown", "logical CPUs": str(os.cpu_count() or 0),
            "page bytes": str(page), "physical memory bytes": memory}


if __name__ == "__main__":
    for key, value in information().items():
        print(f"{key}: {value}")

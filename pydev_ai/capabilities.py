"""Machine-readable collector and implementation capabilities."""

from __future__ import annotations

import importlib.util
import platform
import shutil
from pathlib import Path

from . import SCHEMA_VERSION, __version__


def capabilities() -> dict:
    linux_proc = platform.system() == "Linux" and Path("/proc/stat").is_file()
    return {
        "schema_version": SCHEMA_VERSION,
        "implementation_version": __version__,
        "platform": platform.system(),
        "operations": {
            "get_system_info": {"supported": True, "implementations": ["python", "c"]},
            "sample_system_metrics": {"supported": linux_proc, "reason": None if linux_proc else "requires Linux /proc and /sys"},
            "list_processes": {"supported": linux_proc, "reason": None if linux_proc else "requires Linux /proc"},
            "inspect_process": {"supported": linux_proc, "reason": None if linux_proc else "requires Linux /proc"},
            "list_startup_entries": {"supported": platform.system() in ("Linux", "Darwin")},
            "scan_files": {"supported": True, "python_rules": ["AV-TEST-001", "AV-SIG-001", "AV-MINER-001", "AV-PATH-001"],
                           "c_additional_rules": ["AV-FILE-001", "AV-FILE-002", "AV-FILE-004", "AV-PERSIST-001", "AV-PERSIST-002", "AV-ROOTKIT-001", "AV-MINER-002", "AV-DROPPER-001"]},
            "inspect_file": {"supported": True}, "get_directory_usage": {"supported": True},
            "find_hardlinks": {"supported": True},
            "gpu_telemetry": {"supported": bool(shutil.which("nvidia-smi")), "backend": "nvidia-smi" if shutil.which("nvidia-smi") else None},
            "inference_health": {"supported": True, "enabled": False, "reason": "endpoint must be explicitly configured"},
            "mcp_server": {"supported": importlib.util.find_spec("mcp") is not None, "optional_dependency": "mcp>=2,<3; Python >=3.10"},
            "execution_tracing": {"supported": False, "exposed_via_mcp": False, "reason": "requires separately configured isolation runner"},
        },
        "permissions": {"read_only_collection": True, "network_access": False, "file_writes": False,
                        "package_installation": False, "process_termination": False, "command_execution": False},
    }

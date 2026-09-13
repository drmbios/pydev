"""JSON CLI for structured collectors and deterministic workflows."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import List, Optional

from .collectors import (find_hardlinks, get_capabilities, get_directory_usage,
                         get_system_info, inspect_file, inspect_process,
                         list_processes, list_startup_entries,
                         sample_system_metrics, scan_files)
from .policy import RootPolicy
from .telemetry import InferenceEndpoint
from .workflows import diagnose_system_pressure, inspect_ai_worker_health, investigate_file_indicators


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--root", action="append", default=[], help="authorized filesystem root; repeatable")
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("capabilities"); commands.add_parser("system-info")
    metrics = commands.add_parser("metrics"); metrics.add_argument("--interval", type=float, default=1.0)
    processes = commands.add_parser("processes"); processes.add_argument("--interval", type=float, default=1.0); processes.add_argument("--limit", type=int, default=128)
    process = commands.add_parser("process"); process.add_argument("pid", type=int); process.add_argument("--interval", type=float, default=1.0)
    startup = commands.add_parser("startup"); startup.add_argument("--inspect-contents", action="store_true")
    file_parser = commands.add_parser("file"); file_parser.add_argument("path")
    usage = commands.add_parser("directory-usage"); usage.add_argument("path")
    links = commands.add_parser("hardlinks"); links.add_argument("path"); links.add_argument("search_root")
    scan = commands.add_parser("scan"); scan.add_argument("paths", nargs="+"); scan.add_argument("--database"); scan.add_argument("--max-files", type=int, default=10000)
    pressure = commands.add_parser("diagnose-pressure"); pressure.add_argument("--interval", type=float, default=1.0); pressure.add_argument("--process-limit", type=int, default=32)
    investigate = commands.add_parser("investigate-file"); investigate.add_argument("path"); investigate.add_argument("--database")
    health = commands.add_parser("ai-health"); health.add_argument("--interval", type=float, default=1.0); health.add_argument("--process-limit", type=int, default=32)
    health.add_argument("--allow-network", action="store_true", help="permit the explicitly configured telemetry request")
    health.add_argument("--inference-url", help="HTTP(S) JSON telemetry endpoint")
    health.add_argument("--inference-host", help="exact permitted endpoint hostname")
    return result


def main(argv: Optional[List[str]] = None) -> int:
    args = parser().parse_args(argv)
    policy = RootPolicy(args.root or [str(Path.cwd())])
    dispatch = {
        "capabilities": lambda: get_capabilities(), "system-info": lambda: get_system_info(),
        "metrics": lambda: sample_system_metrics(args.interval),
        "processes": lambda: list_processes(args.interval, args.limit),
        "process": lambda: inspect_process(args.pid, args.interval),
        "startup": lambda: list_startup_entries(policy, args.inspect_contents),
        "file": lambda: inspect_file(policy, args.path),
        "directory-usage": lambda: get_directory_usage(policy, args.path),
        "hardlinks": lambda: find_hardlinks(policy, args.path, args.search_root),
        "scan": lambda: scan_files(policy, args.paths, args.database, args.max_files),
        "diagnose-pressure": lambda: diagnose_system_pressure(args.interval, args.process_limit),
        "investigate-file": lambda: investigate_file_indicators(policy, args.path, args.database),
        "ai-health": lambda: inspect_ai_worker_health(
            args.interval, args.process_limit,
            InferenceEndpoint(args.inference_url, args.inference_host)
            if args.inference_url and args.inference_host else None,
            args.allow_network),
    }
    response = dispatch[args.command]()
    print(json.dumps(response, indent=2, sort_keys=True))
    return 0 if response["status"] in ("success", "partial", "unsupported") else 1


if __name__ == "__main__": raise SystemExit(main())

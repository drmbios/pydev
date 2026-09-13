"""Read-only collectors returning the versioned response contract."""

from __future__ import annotations

import hashlib
import os
import platform
import stat
from pathlib import Path
from typing import List, Optional

import antivermis
import autostartx
import coreinfo
import linkscan
import lsx
import stringsx

from .capabilities import capabilities
from .contract import ResponseBuilder, StructuredError, execute
from .metrics import sample_system
from .policy import RootPolicy, redact
from .processes import process_metadata, sample_processes

MAX_FILE_BYTES = 16 * 1024 * 1024


def get_capabilities(request_id: Optional[str] = None) -> dict:
    return execute("get_capabilities", lambda b: b.finish(capabilities(), capabilities=capabilities()), request_id)


def get_system_info(request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        raw = coreinfo.information()
        data = {
            "system": raw["system"], "kernel": raw["kernel"],
            "architecture": raw["architecture"], "cpu": raw["cpu"],
            "logical_cpus": int(raw["logical CPUs"]),
            "page_size_bytes": int(raw["page bytes"]),
            "physical_memory_bytes": None if raw["physical memory bytes"] == "unknown"
            else int(raw["physical memory bytes"]),
        }
        evidence_id = builder.add_evidence("system_info", data, "platform/os")
        data["evidence_id"] = evidence_id
        return builder.finish(data, capabilities={"cross_platform": True})
    return execute("get_system_info", operation, request_id)


def sample_system_metrics(interval: float = 1.0, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        if platform.system() != "Linux": raise NotImplementedError("system rates require Linux /proc and /sys")
        data = sample_system(interval)
        builder.warnings.extend(data.pop("warnings"))
        evidence_id = builder.add_evidence("sample_window", {"duration_seconds": data["sample_duration_seconds"]}, "monotonic clock")
        data["evidence_id"] = evidence_id
        return builder.finish(data, capabilities={"rates": True, "units_explicit": True})
    return execute("sample_system_metrics", operation, request_id)


def list_processes(interval: float = 1.0, limit: int = 256, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        if not 1 <= limit <= 4096: raise ValueError("process limit must be 1-4096")
        sample = sample_processes(interval, max_processes=4096)
        rows = sample["processes"][:limit]
        builder.coverage.scanned = len(rows)
        builder.coverage.skipped = sample["skipped"] + max(0, len(sample["processes"]) - limit)
        builder.coverage.failed = sample["failed"]
        if len(sample["processes"]) > limit:
            builder.coverage.truncated = len(sample["processes"]) - limit
            builder.coverage.limit_reasons.append("process result limit")
        for row in rows:
            row["evidence_id"] = builder.add_evidence("process_sample", {"pid": row["pid"], "start_identity_ticks": row["start_identity_ticks"]}, "Linux /proc")
        sample["processes"] = rows
        return builder.finish(sample)
    return execute("list_processes", operation, request_id)


def inspect_process(pid: int, interval: float = 1.0, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        if not 1 <= pid <= 2_147_483_647: raise ValueError("invalid PID")
        sample = sample_processes(interval, max_processes=4096)
        row = next((item for item in sample["processes"] if item["pid"] == pid), None)
        if row is None:
            builder.errors.append(StructuredError("process_unavailable", "process disappeared, was reused, or was outside collection coverage"))
            return builder.finish({"pid": pid}, "partial")
        row["evidence_id"] = builder.add_evidence("process_sample", {"pid": pid, "start_identity_ticks": row["start_identity_ticks"]}, "Linux /proc")
        return builder.finish(row, capabilities={"environment_values_exposed": False})
    return execute("inspect_process", operation, request_id)


def _bounded_hash(path: Path, max_bytes: int) -> tuple[str, bytes, os.stat_result]:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
    descriptor = os.open(path, flags); digest = hashlib.sha256(); sample = bytearray(); total = 0
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode): raise ValueError("path is not a regular file")
        while True:
            chunk = os.read(descriptor, min(65_536, max_bytes + 1 - total))
            if not chunk: break
            total += len(chunk)
            if total > max_bytes: raise ValueError("file exceeds byte limit during read")
            digest.update(chunk)
            if len(sample) < 65_536: sample.extend(chunk[:65_536 - len(sample)])
        after = os.fstat(descriptor)
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
            raise OSError("file changed during read")
        return digest.hexdigest(), bytes(sample), after
    finally: os.close(descriptor)


def inspect_file(policy: RootPolicy, path: str, max_bytes: int = MAX_FILE_BYTES,
                 string_minimum: int = 4, hex_bytes: int = 256, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        if not 1 <= max_bytes <= MAX_FILE_BYTES or not 1 <= hex_bytes <= 4096 or not 1 <= string_minimum <= 4096:
            raise ValueError("invalid file evidence limits")
        authorized = policy.authorize(path)
        digest, sample, info = _bounded_hash(authorized, max_bytes)
        printable = stringsx.strings(sample, string_minimum)[:128]
        redacted, count = redact("\n".join(printable))
        if count: builder.warnings.append("{} likely secret value(s) redacted; redaction is heuristic".format(count))
        evidence_id = builder.add_evidence("file_identity", {"sha256": digest, "device": info.st_dev, "inode": info.st_ino}, str(authorized))
        builder.coverage.scanned = 1
        return builder.finish({"path": str(authorized), "type": "regular_file", "size_bytes": info.st_size,
            "mode_octal": "{:03o}".format(stat.S_IMODE(info.st_mode)), "mtime_ns": info.st_mtime_ns,
            "sha256": digest, "crc32_security_use": False, "hex_preview": sample[:hex_bytes].hex(),
            "printable_strings": redacted.splitlines(), "evidence_id": evidence_id}, capabilities=policy.capabilities())
    return execute("inspect_file", operation, request_id)


def get_directory_usage(policy: RootPolicy, path: str, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        authorized = policy.authorize(path)
        size = lsx.real_size(authorized)
        builder.coverage.scanned = 1
        evidence_id = builder.add_evidence("directory_usage", {"apparent_size_bytes": size}, str(authorized))
        return builder.finish({"path": str(authorized), "apparent_size_bytes": size, "evidence_id": evidence_id}, capabilities=policy.capabilities())
    return execute("get_directory_usage", operation, request_id)


def find_hardlinks(policy: RootPolicy, path: str, root: str, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        target, search_root = policy.authorize(path), policy.authorize(root)
        matches = linkscan.find_links(target, search_root)
        builder.coverage.scanned = len(matches)
        evidence_id = builder.add_evidence("file_identity", {"paths": [str(item) for item in matches]}, str(target))
        return builder.finish({"target": str(target), "search_root": str(search_root), "paths": [str(item) for item in matches], "evidence_id": evidence_id}, capabilities=policy.capabilities())
    return execute("find_hardlinks", operation, request_id)


def list_startup_entries(policy: RootPolicy, inspect_contents: bool = False,
                         request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        entries = []
        candidates = []
        for location in autostartx.locations():
            try: authorized = policy.authorize(str(location))
            except (FileNotFoundError, PermissionError):
                builder.coverage.skipped += 1
                continue
            try:
                candidates.extend(Path(entry.path) for entry in os.scandir(authorized)
                                  if not entry.is_symlink())
            except OSError as error:
                builder.coverage.failed += 1
                builder.errors.append(StructuredError("startup_list_failed", str(error)))
        for path in candidates[:10_000]:
            try: authorized = policy.authorize(str(path))
            except (FileNotFoundError, PermissionError): builder.coverage.skipped += 1; continue
            try:
                info = authorized.lstat(); entry = {"path": str(authorized), "listed": True, "content_inspected": False,
                    "malicious": None, "mode_octal": "{:03o}".format(stat.S_IMODE(info.st_mode))}
                if inspect_contents and stat.S_ISREG(info.st_mode):
                    _, sample, _ = _bounded_hash(authorized, 64 * 1024); text, count = redact(sample.decode("utf-8", "replace"))
                    entry["content_inspected"] = True; entry["content_preview"] = text[:4096]
                    if count: builder.warnings.append("likely secret redacted in startup entry")
                entry["evidence_id"] = builder.add_evidence("startup_entry", {"listed": True, "content_inspected": entry["content_inspected"]}, str(authorized))
                entries.append(entry); builder.coverage.scanned += 1
            except OSError as error:
                builder.coverage.failed += 1; builder.errors.append(StructuredError("startup_read_failed", str(error)))
        return builder.finish({"entries": entries, "interpretation": "listing proves only that an entry exists, not that it is active or malicious"}, capabilities=policy.capabilities())
    return execute("list_startup_entries", operation, request_id)


RULE_INVENTORY = {
    "AV-TEST-001": ("informational", "Harmless EICAR anti-malware test file"),
    "AV-SIG-001": ("high", "SHA-256 signature database match"),
    "AV-MINER-001": ("high", "Compound Stratum and miner indicators"),
    "AV-PATH-001": ("medium", "Executable in temporary storage"),
}


def scan_files(policy: RootPolicy, paths: List[str], database: Optional[str] = None,
               max_files: int = 10_000, max_bytes_per_file: int = 16 * 1024 * 1024,
               request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        if not paths or len(paths) > 64 or not 1 <= max_files <= 100_000 or not 1 <= max_bytes_per_file <= 64 * 1024 * 1024:
            raise ValueError("invalid scan limits")
        authorized = [policy.authorize(path) for path in paths]
        db_path = policy.authorize(database) if database else None
        result = antivermis.scan(authorized, antivermis.load_database(db_path), max_files, max_bytes_per_file)
        findings = []
        for rule_id, path_value, detail in result.findings:
            severity, description = RULE_INVENTORY.get(rule_id, ("unknown", "Implementation-specific rule"))
            evidence_id = builder.add_evidence("security_indicator", detail, str(path_value), rule_id=rule_id)
            findings.append({"rule_id": rule_id, "severity": severity, "path": str(path_value),
                             "evidence": detail, "description": description, "evidence_id": evidence_id})
        builder.coverage.scanned = result.files; builder.coverage.failed = result.errors
        if result.limited: builder.coverage.truncated += 1; builder.coverage.limit_reasons.append("scan work limit")
        if result.errors: builder.errors.append(StructuredError("scan_incomplete", "one or more paths could not be scanned"))
        return builder.finish({"findings": findings, "finding_count": len(findings), "bytes_scanned": result.bytes,
            "rule_inventory": [{"rule_id": key, "severity": value[0], "description": value[1]} for key, value in sorted(RULE_INVENTORY.items())],
            "safety_statement": "No findings does not prove that a file or host is safe."}, capabilities={**policy.capabilities(), "read_only": True, "implementation": "python"})
    return execute("scan_files", operation, request_id)

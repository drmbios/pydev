"""Deterministic, evidence-linked investigation workflows."""

from __future__ import annotations

import platform
from pathlib import Path
from typing import List, Optional

from .collectors import inspect_file, list_processes, list_startup_entries, sample_system_metrics, scan_files
from .contract import ResponseBuilder, StructuredError, execute
from .policy import RootPolicy
from .telemetry import InferenceEndpoint, collect_inference, collect_nvidia


def _child_problem(builder: ResponseBuilder, name: str, response: dict) -> None:
    if response["status"] != "success":
        builder.warnings.append("{} returned {}".format(name, response["status"]))
    for error in response.get("errors", []):
        builder.errors.append(StructuredError("{}_{}".format(name, error["code"]), error["message"]))


def diagnose_system_pressure(interval: float = 1.0, process_limit: int = 32,
                             request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        metrics = sample_system_metrics(interval)
        processes = list_processes(interval, process_limit)
        _child_problem(builder, "metrics", metrics); _child_problem(builder, "processes", processes)
        facts, hypotheses, missing, checks = [], [], [], []
        metric_data = metrics.get("data", {})
        cpu = metric_data.get("cpu", {}).get("utilization_percent")
        memory = metric_data.get("memory", {}).get("utilization_percent")
        iowait = metric_data.get("cpu", {}).get("iowait_percent")
        metric_evidence_id = builder.add_evidence(
            "workflow_metric_sample",
            {"cpu": metric_data.get("cpu"), "memory": metric_data.get("memory")},
            "sample_system_metrics") if cpu is not None or memory is not None else None
        if cpu is not None:
            facts.append({"fact": "sampled CPU utilization", "value": cpu, "unit": "percent", "evidence_id": metric_evidence_id})
            if cpu >= 85: hypotheses.append({"hypothesis": "CPU pressure was observed during the sample window", "confidence": "high", "evidence_ids": [metric_evidence_id]})
        if memory is not None:
            facts.append({"fact": "memory utilization", "value": memory, "unit": "percent", "evidence_id": metric_evidence_id})
            if memory >= 90: hypotheses.append({"hypothesis": "memory pressure may be present", "confidence": "medium", "evidence_ids": [metric_evidence_id]})
        if iowait is not None and iowait >= 20: hypotheses.append({"hypothesis": "storage latency may contribute to pressure", "confidence": "medium", "evidence_ids": [metric_evidence_id]})
        if not hypotheses and metric_evidence_id:
            hypotheses.append({"hypothesis": "no high-pressure threshold was crossed during this bounded window", "confidence": "limited", "evidence_ids": [metric_evidence_id]})
        if metric_evidence_id is None:
            missing.append("Host pressure metrics were unavailable on this platform or permission boundary.")
        missing.append("Historical attribution before the sample window is unavailable.")
        checks.extend(["Repeat with a longer approved sample window.", "Correlate high-CPU PIDs with service logs and workload telemetry."])
        return builder.finish({"observed_facts": facts, "process_measurements": processes.get("data", {}).get("processes", []),
            "hypotheses": hypotheses, "missing_evidence": missing, "suggested_next_checks": checks,
            "source_reports": {"metrics": metrics, "processes": processes}})
    return execute("diagnose_system_pressure", operation, request_id)


def investigate_file_indicators(policy: RootPolicy, path: str, database: Optional[str] = None,
                                interval: float = 0.5, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        file_report = inspect_file(policy, path)
        scan_report = scan_files(policy, [path], database)
        startup_report = list_startup_entries(policy)
        process_report = list_processes(interval, 512) if platform.system() == "Linux" else None
        for name, report in (("file", file_report), ("scan", scan_report), ("startup", startup_report)):
            _child_problem(builder, name, report)
        target = file_report.get("data", {}).get("path")
        process_matches = []
        if process_report:
            _child_problem(builder, "processes", process_report)
            process_matches = [row for row in process_report.get("data", {}).get("processes", []) if row.get("executable") == target]
        startup_matches = [entry for entry in startup_report.get("data", {}).get("entries", []) if entry.get("path") == target]
        facts = {"file": file_report.get("data", {}), "security_findings": scan_report.get("data", {}).get("findings", []),
                 "matching_running_executables": process_matches, "matching_startup_entries": startup_matches}
        interpretations = []
        if facts["security_findings"]: interpretations.append({"statement": "One or more bounded rules matched; manual validation is required.", "basis": "security_findings"})
        if process_matches: interpretations.append({"statement": "The same resolved path was observed as a running executable during the sample.", "basis": "matching_running_executables"})
        return builder.finish({"observed_facts": facts, "interpretations": interpretations,
            "not_observed": ["execution history", "causal ownership", "network connections"],
            "source_reports": {"file": file_report, "scan": scan_report, "startup": startup_report, "processes": process_report}})
    return execute("investigate_file_indicators", operation, request_id)


def inspect_ai_worker_health(interval: float = 1.0, process_limit: int = 32,
                             inference_endpoint: Optional[InferenceEndpoint] = None,
                             allow_network: bool = False, request_id: Optional[str] = None) -> dict:
    def operation(builder: ResponseBuilder) -> dict:
        metrics = sample_system_metrics(interval)
        processes = list_processes(interval, process_limit)
        gpu = collect_nvidia()
        inference = collect_inference(inference_endpoint) if allow_network else {
            "status": "unsupported", "enabled": False, "reason": "network telemetry permission disabled"}
        _child_problem(builder, "metrics", metrics); _child_problem(builder, "processes", processes)
        if gpu["status"] != "success": builder.warnings.append("GPU telemetry: {}".format(gpu.get("reason")))
        if inference["status"] != "success": builder.warnings.append("Inference telemetry: {}".format(inference.get("reason")))
        return builder.finish({"host_metrics": metrics, "process_metrics": processes, "gpu": gpu,
            "inference_service": inference, "interpretation": "Unavailable metrics remain explicit and are never replaced with zeroes or estimates."})
    return execute("inspect_ai_worker_health", operation, request_id)

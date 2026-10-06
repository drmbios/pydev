"""Versioned response contract shared by CLI workflows and MCP tools."""

from __future__ import annotations

import platform
import time
import uuid
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Callable, Dict, List, Optional

from . import SCHEMA_VERSION, __version__

STATUSES = {"success", "partial", "unsupported", "failure"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


@dataclass
class Coverage:
    scanned: int = 0
    skipped: int = 0
    failed: int = 0
    truncated: int = 0
    limit_reasons: List[str] = field(default_factory=list)


@dataclass
class StructuredError:
    code: str
    message: str
    evidence_id: Optional[str] = None


@dataclass
class Response:
    tool: str
    request_id: str
    observed_at_utc: str
    completed_at_utc: str
    duration_ms: float
    status: str
    data: Dict[str, Any]
    evidence: List[Dict[str, Any]]
    warnings: List[str]
    errors: List[StructuredError]
    coverage: Coverage
    capabilities: Dict[str, Any]
    schema_version: str = SCHEMA_VERSION
    implementation_version: str = __version__
    platform: Dict[str, str] = field(default_factory=lambda: {
        "system": platform.system(), "release": platform.release(), "machine": platform.machine()
    })

    def to_dict(self) -> Dict[str, Any]:
        result = asdict(self)
        if result["status"] not in STATUSES:
            raise ValueError("invalid response status")
        return result


class ResponseBuilder:
    def __init__(self, tool: str, request_id: Optional[str] = None) -> None:
        self.tool = tool
        self.request_id = request_id or str(uuid.uuid4())
        self.started_utc = utc_now()
        self.started = time.monotonic()
        self.evidence: List[Dict[str, Any]] = []
        self.warnings: List[str] = []
        self.errors: List[StructuredError] = []
        self.coverage = Coverage()

    def add_evidence(self, kind: str, value: Any, source: str, **metadata: Any) -> str:
        identifier = "E{:04d}".format(len(self.evidence) + 1)
        item = {"evidence_id": identifier, "kind": kind, "source": source, "value": value}
        item.update(metadata)
        self.evidence.append(item)
        return identifier

    def finish(self, data: Dict[str, Any], status: Optional[str] = None,
               capabilities: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        if status is None:
            status = "partial" if self.errors or self.coverage.failed or self.coverage.truncated else "success"
        response = Response(
            tool=self.tool, request_id=self.request_id, observed_at_utc=self.started_utc,
            completed_at_utc=utc_now(), duration_ms=round((time.monotonic() - self.started) * 1000.0, 3),
            status=status, data=data, evidence=self.evidence, warnings=self.warnings,
            errors=self.errors, coverage=self.coverage, capabilities=capabilities or {},
        )
        return response.to_dict()


def execute(tool: str, operation: Callable[[ResponseBuilder], Dict[str, Any]],
            request_id: Optional[str] = None) -> Dict[str, Any]:
    builder = ResponseBuilder(tool, request_id)
    try:
        return operation(builder)
    except NotImplementedError as error:
        builder.errors.append(StructuredError("unsupported", str(error)))
        return builder.finish({}, "unsupported")
    except Exception as error:  # Boundary converts internal failure to structured data.
        builder.errors.append(StructuredError("execution_failure", str(error)))
        return builder.finish({}, "failure")


RESPONSE_SCHEMA: Dict[str, Any] = {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "$id": "https://drmbios.github.io/pydev/schema/response-1.0.json",
    "type": "object",
    "required": ["schema_version", "tool", "implementation_version", "request_id",
                 "observed_at_utc", "completed_at_utc", "duration_ms", "platform",
                 "capabilities", "status", "data", "evidence", "warnings", "errors", "coverage"],
    "properties": {
        "schema_version": {"const": SCHEMA_VERSION},
        "status": {"enum": sorted(STATUSES)},
        "duration_ms": {"type": "number", "minimum": 0},
        "data": {"type": "object"}, "evidence": {"type": "array"},
        "warnings": {"type": "array", "items": {"type": "string"}},
        "errors": {"type": "array"}, "coverage": {"type": "object"},
    },
}

"""Capability-based optional GPU and explicitly configured service telemetry."""

from __future__ import annotations

import json
import shutil
import subprocess
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Optional
from urllib.parse import urlparse

MAX_RESPONSE = 1024 * 1024


def collect_nvidia(timeout: float = 5.0) -> dict:
    binary = shutil.which("nvidia-smi")
    if not binary:
        return {"status": "unsupported", "backend": "nvidia-smi", "reason": "nvidia-smi not installed"}
    command = [binary, "--query-gpu=index,name,utilization.gpu,memory.total,memory.used", "--format=csv,noheader,nounits"]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout, check=False)
    except subprocess.TimeoutExpired:
        return {"status": "failure", "backend": "nvidia-smi", "reason": "probe timed out and was terminated"}
    if result.returncode != 0:
        return {"status": "failure", "backend": "nvidia-smi", "reason": result.stderr[:1024] or "probe failed"}
    devices = []
    for line in result.stdout.splitlines()[:64]:
        fields = [field.strip() for field in line.split(",")]
        if len(fields) != 5: continue
        devices.append({"index": int(fields[0]), "name": fields[1], "utilization_percent": float(fields[2]),
                        "vram_total_bytes": int(fields[3]) * 1024 * 1024, "vram_used_bytes": int(fields[4]) * 1024 * 1024})
    return {"status": "success", "backend": "nvidia-smi", "devices": devices}


class SameOriginRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):  # type: ignore[no-untyped-def]
        old, new = urlparse(request.full_url), urlparse(new_url)
        if (old.scheme, old.hostname, old.port) != (new.scheme, new.hostname, new.port):
            raise urllib.error.HTTPError(new_url, code, "cross-origin redirect refused", headers, fp)
        return super().redirect_request(request, fp, code, message, headers, new_url)


@dataclass(frozen=True)
class InferenceEndpoint:
    url: str
    allowed_host: str

    def validate(self) -> None:
        parsed = urlparse(self.url)
        if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.hostname != self.allowed_host:
            raise ValueError("inference endpoint must use HTTP(S) and match the configured host")
        if parsed.username or parsed.password or parsed.fragment:
            raise ValueError("credentials and fragments are not permitted in telemetry URLs")


def collect_inference(endpoint: Optional[InferenceEndpoint], timeout: float = 3.0) -> dict:
    if endpoint is None:
        return {"status": "unsupported", "enabled": False, "reason": "no inference endpoint configured"}
    endpoint.validate()
    opener = urllib.request.build_opener(SameOriginRedirect())
    request = urllib.request.Request(endpoint.url, headers={"Accept": "application/json", "User-Agent": "pydev-ai/1"})
    try:
        with opener.open(request, timeout=timeout) as response:
            payload = response.read(MAX_RESPONSE + 1)
    except (OSError, urllib.error.URLError) as error:
        return {"status": "failure", "enabled": True, "reason": str(error)}
    if len(payload) > MAX_RESPONSE:
        return {"status": "failure", "enabled": True, "reason": "telemetry response exceeds limit"}
    try: data = json.loads(payload.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError): return {"status": "failure", "enabled": True, "reason": "invalid JSON response"}
    allowed = {}
    for key, unit in (("queue_depth", "requests"), ("request_latency_ms", "milliseconds"),
                      ("throughput_requests_per_second", "requests/second"), ("tokens_per_second", "tokens/second")):
        if key in data and isinstance(data[key], (int, float)):
            allowed[key] = {"value": data[key], "unit": unit}
    return {"status": "success", "enabled": True, "metrics": allowed,
            "warning": "Only service-exported values are reported; missing metrics are not estimated."}

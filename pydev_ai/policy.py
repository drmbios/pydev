"""Filesystem authorization and secret redaction at the adapter boundary."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable, List

SECRET_PATTERNS = [
    re.compile(r"-----BEGIN ([A-Z ]*PRIVATE KEY)-----[\s\S]*?(?:-----END \1-----|\Z)"),
    re.compile(r"(?i)\b(api[_-]?key|secret|token|password)\b\s*[:=]\s*([^\s,;]{6,})"),
    re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(r"\bgh[pousr]_[A-Za-z0-9_]{20,}\b"),
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
]


class RootPolicy:
    def __init__(self, roots: Iterable[str]) -> None:
        resolved: List[Path] = []
        for root in roots:
            candidate = Path(root).expanduser().resolve(strict=True)
            if not candidate.is_dir():
                raise ValueError("configured root is not a directory: {}".format(root))
            resolved.append(candidate)
        if not resolved:
            raise ValueError("at least one filesystem root is required")
        self.roots = tuple(resolved)

    def authorize(self, value: str) -> Path:
        candidate = Path(value).expanduser().resolve(strict=True)
        for root in self.roots:
            try:
                candidate.relative_to(root)
                return candidate
            except ValueError:
                continue
        raise PermissionError("path is outside configured roots")

    def capabilities(self) -> dict:
        return {"filesystem_roots": [str(root) for root in self.roots], "symlink_escape_protection": True}


def redact(text: str) -> tuple[str, int]:
    redactions = 0
    for pattern in SECRET_PATTERNS:
        def replace(match: re.Match) -> str:
            nonlocal redactions
            redactions += 1
            return (match.group(1) + "=[REDACTED]") if match.lastindex and match.lastindex >= 2 else "[REDACTED]"
        text = pattern.sub(replace, text)
    return text, redactions

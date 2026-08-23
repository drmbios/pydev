"""Read-only, bounded threat-indicator scanner and signature updater.

This is an independent Python counterpart to the native C tool. Findings are
evidence for investigation, not a guarantee that malware is or is not present.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import ssl
import stat
import tempfile
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import urlparse
from pycommon import read_bytes

EICAR_SHA256 = "275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f"
MAX_DATABASE = 16 * 1024 * 1024
MAX_MANIFEST = 64 * 1024
MAX_SIGNATURES = 4096
MAX_DEPTH = 64
MAX_FINDINGS = 10_000


@dataclass(frozen=True)
class Signature:
    digest: str
    size: int | None
    label: str


@dataclass
class ScanResult:
    findings: list[tuple[str, Path, str]] = field(default_factory=list)
    files: int = 0
    bytes: int = 0
    errors: int = 0
    limited: bool = False


def parse_database(data: bytes) -> dict[str, list[Signature]]:
    if len(data) > MAX_DATABASE or b"\0" in data:
        raise ValueError("signature database is oversized or contains NUL")
    signatures: dict[str, list[Signature]] = {}
    for raw in data.decode("ascii", "strict").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line == "ANTIVERMIS-SIGDB 1": continue
        clam = re.fullmatch(r"([0-9A-Fa-f]{64}):(\*|\d+):([^\t\r\n]{1,128})", line)
        plain = re.fullmatch(r"([0-9A-Fa-f]{64})[ \t]+([^\t\r\n]{1,128})", line)
        if clam: digest, size, label = clam.group(1).lower(), clam.group(2), clam.group(3); expected = None if size == "*" else int(size)
        elif plain: digest, label, expected = plain.group(1).lower(), plain.group(2), None
        else: raise ValueError("malformed signature record")
        signatures.setdefault(digest, []).append(Signature(digest, expected, label))
        if sum(map(len, signatures.values())) > MAX_SIGNATURES: raise ValueError("too many signatures")
    return signatures


def load_database(path: Path | None) -> dict[str, list[Signature]]:
    if path is None: return {}
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_size > MAX_DATABASE: raise ValueError("database must be a bounded regular file")
    return parse_database(read_bytes(path, MAX_DATABASE))


def hash_file(path: Path, limit: int) -> tuple[str, bytes, int]:
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
    descriptor = os.open(path, flags); digest = hashlib.sha256(); sample = bytearray(); total = 0
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit: raise ValueError("file exceeds scan limit")
        while chunk := os.read(descriptor, 65_536):
            total += len(chunk); digest.update(chunk)
            if len(sample) < 65_536: sample.extend(chunk[:65_536 - len(sample)])
        after = os.fstat(descriptor)
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
            raise OSError("file changed during scan")
        return digest.hexdigest(), bytes(sample), total
    finally: os.close(descriptor)


def scan(paths: list[Path], signatures: dict[str, list[Signature]], max_files: int = 100_000,
         max_file_bytes: int = 32 * 1024 * 1024, max_total: int = 1024 * 1024 * 1024) -> ScanResult:
    result = ScanResult(); stack = [(path, 0) for path in reversed(paths)]; entries = 0
    while stack and not result.limited:
        path, depth = stack.pop()
        entries += 1
        if entries > max_files: result.limited = True; break
        try: info = path.lstat()
        except OSError: result.errors += 1; continue
        if stat.S_ISLNK(info.st_mode) or not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)): continue
        if stat.S_ISDIR(info.st_mode):
            if depth >= MAX_DEPTH: result.limited = True; continue
            try:
                children = []
                with os.scandir(path) as directory:
                    for entry in directory:
                        if len(children) + len(stack) + entries >= max_files: result.limited = True; break
                        if not entry.is_symlink(): children.append(Path(entry.path))
                stack.extend((child, depth + 1) for child in reversed(children))
            except OSError: result.errors += 1
            continue
        if result.files >= max_files or result.bytes + info.st_size > max_total: result.limited = True; break
        result.files += 1
        try: digest, sample, size = hash_file(path, max_file_bytes)
        except (OSError, ValueError): result.errors += 1; continue
        result.bytes += size
        if digest == EICAR_SHA256: result.findings.append(("AV-TEST-001", path, "harmless EICAR test signature"))
        for signature in signatures.get(digest, []):
            if signature.size is None or signature.size == size: result.findings.append(("AV-SIG-001", path, signature.label))
        lower = sample.lower()
        if b"stratum+tcp://" in lower and any(token in lower for token in (b"xmrig", b"minerd", b"donate-level")):
            result.findings.append(("AV-MINER-001", path, "compound miner/Stratum indicators"))
        if info.st_mode & stat.S_IXUSR and str(path).startswith(("/tmp/", "/var/tmp/", "/dev/shm/")):
            result.findings.append(("AV-PATH-001", path, "executable in temporary storage"))
        if len(result.findings) >= MAX_FINDINGS: result.limited = True
    return result


def _download(url: str, limit: int) -> bytes:
    parsed = urlparse(url)
    if parsed.scheme not in ("https", "file") or any(ord(character) <= 32 or ord(character) == 127 for character in url):
        raise ValueError("update URL must use HTTPS or file:// without control characters")
    request = urllib.request.Request(url, headers={"User-Agent": "antivermis-python/1"})
    with urllib.request.urlopen(request, timeout=60, context=ssl.create_default_context()) as response:
        if urlparse(response.geturl()).scheme not in ("https", "file"): raise ValueError("unsafe update redirect")
        data = response.read(limit + 1)
    if len(data) > limit: raise ValueError("update exceeds size limit")
    return data


def parse_manifest(data: bytes) -> tuple[str, str, str]:
    if len(data) > MAX_MANIFEST or b"\0" in data: raise ValueError("invalid manifest")
    lines = [line for line in data.decode("ascii", "strict").splitlines() if line and not line.startswith("#")]
    if not lines or lines.pop(0) != "ANTIVERMIS-MANIFEST 1": raise ValueError("invalid manifest header")
    values = {}
    for line in lines:
        key, separator, value = line.partition(" ")
        if not separator or key in values or key not in ("version", "database", "sha256"): raise ValueError("invalid manifest field")
        values[key] = value
    if set(values) != {"version", "database", "sha256"}: raise ValueError("incomplete manifest")
    if not re.fullmatch(r"[A-Za-z0-9._+-]{1,63}", values["version"]): raise ValueError("invalid version")
    if not re.fullmatch(r"[0-9A-Fa-f]{64}", values["sha256"]): raise ValueError("invalid checksum")
    if urlparse(values["database"]).scheme not in ("https", "file") or any(ord(character) <= 32 or ord(character) == 127 for character in values["database"]):
        raise ValueError("invalid database URL")
    return values["version"], values["database"], values["sha256"].lower()


def update_database(manifest_url: str, destination: Path, check_only: bool = False) -> str:
    version, database_url, expected = parse_manifest(_download(manifest_url, MAX_MANIFEST))
    try: current = hashlib.sha256(read_bytes(destination, MAX_DATABASE)).hexdigest() if destination.is_file() and not destination.is_symlink() else None
    except OSError: current = None
    if current == expected: return f"database is current (version {version})"
    if check_only: return f"database update available (version {version})"
    try:
        destination_info = destination.lstat()
        if not stat.S_ISREG(destination_info.st_mode): raise ValueError("destination must be a regular non-symlink file")
    except FileNotFoundError:
        pass
    database = _download(database_url, MAX_DATABASE)
    if hashlib.sha256(database).hexdigest() != expected: raise ValueError("downloaded database SHA-256 mismatch")
    parse_database(database)
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=destination.name + ".tmp.", dir=destination.parent)
    try:
        os.fchmod(descriptor, 0o600)
        written = 0
        while written < len(database):
            amount = os.write(descriptor, database[written:])
            if amount <= 0: raise OSError("short database write")
            written += amount
        os.fsync(descriptor); os.close(descriptor); descriptor = -1
        os.replace(temporary, destination)
    finally:
        if descriptor >= 0: os.close(descriptor)
        try: os.unlink(temporary)
        except FileNotFoundError: pass
    return f"database updated to version {version}"


def system_paths() -> list[Path]:
    if __import__("platform").system() == "Darwin": return [Path("/Library/LaunchAgents"), Path("/Library/LaunchDaemons"), Path("/tmp")]
    return [Path("/etc/ld.so.preload"), Path("/etc/systemd/system"), Path("/etc/cron.d"), Path("/tmp"), Path("/dev/shm")]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("paths", nargs="*"); parser.add_argument("--db", type=Path)
    parser.add_argument("--system", action="store_true"); parser.add_argument("--max-files", type=int, default=100_000); parser.add_argument("--max-bytes", type=int, default=32)
    parser.add_argument("--check-update", nargs=2, metavar=("MANIFEST_URL", "DATABASE")); parser.add_argument("--update-db", nargs=2, metavar=("MANIFEST_URL", "DATABASE"))
    args = parser.parse_args(argv)
    try:
        action = args.check_update or args.update_db
        if action: print(update_database(action[0], Path(action[1]), bool(args.check_update))); return 0
        paths = [Path(value) for value in args.paths] + (system_paths() if args.system else [])
        if not paths or not 1 <= args.max_files <= 1_000_000 or not 1 <= args.max_bytes <= 64: raise ValueError("invalid or missing scan paths/limits")
        result = scan(paths, load_database(args.db), args.max_files, args.max_bytes * 1024 * 1024)
        for rule, path, evidence in result.findings: print(f"{rule} {path}: {evidence}")
        print(f"files={result.files} bytes={result.bytes} findings={len(result.findings)} errors={result.errors}")
        return 2 if result.limited or result.errors else 1 if result.findings else 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"antivermis: {error}", file=__import__("sys").stderr); return 2


if __name__ == "__main__": raise SystemExit(main())

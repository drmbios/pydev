# Antivermis rule inventory

Rule identifiers are stable evidence labels, not malware-family verdicts.
Findings require manual validation, and a clean result never proves safety.

| Rule | Severity | Meaning | C | Python |
| --- | --- | --- | --- | --- |
| `AV-TEST-001` | informational/test | Harmless EICAR test signature | yes | yes |
| `AV-SIG-001` | high | Configured SHA-256 signature matched | yes | yes |
| `AV-MINER-001` | high | Compound Stratum and miner indicators | yes | yes |
| `AV-MINER-002` | low | Stratum endpoint requiring context | yes | no |
| `AV-DROPPER-001` | high | Compound download/shell/execute indicators | yes | no |
| `AV-FILE-001` | high | World-writable executable | yes | no |
| `AV-FILE-002` | high | Set-ID executable outside normal binary paths | yes | no |
| `AV-FILE-003` | medium | Executable in globally writable temporary storage | yes | no |
| `AV-PATH-001` | medium | Executable in temporary storage | no | yes |
| `AV-FILE-004` | medium | Executable with document-style double extension | yes | no |
| `AV-PERSIST-001` | medium | World-writable persistence entry | yes | no |
| `AV-PERSIST-002` | medium | Executable directly in a persistence directory | yes | no |
| `AV-ROOTKIT-001` | medium | Non-empty Linux loader-preload configuration | yes | no |

The differing C/Python sets are also returned by `get_capabilities`. Both
implementations stream bounded regular files, refuse symlinks and special
files, support local SHA-256 databases, and do not delete or execute content.
The C scanner remains the richer host-hunting implementation; the Python
adapter provides the smaller cross-platform subset and structured orchestration.


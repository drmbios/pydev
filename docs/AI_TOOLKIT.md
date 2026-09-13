# AI-assisted operations toolkit

`pydev_ai` is a read-only adapter over selected pydev collectors. It gives
operators and local AI clients one stable JSON contract without replacing the
existing C and Python commands.

## Install and run

The ordinary commands require Python 3.9+. The optional MCP server uses the
official Python SDK v2 and requires Python 3.10+:

```sh
python3 -m pip install -e .
python3 -m pydev_ai capabilities
python3 -m pydev_ai system-info
python3 -m pydev_ai metrics --interval 1
python3 -m pydev_ai file --root "$PWD" README.md
python3 -m pydev_ai diagnose-pressure --interval 1
```

Install MCP support only when a local client needs it:

```sh
python3 -m pip install -e '.[mcp]'
python3 -m pydev_ai.mcp_server --root "$PWD"
```

## Contract and metric semantics

Every adapter response has `schema_version`, tool and implementation versions,
a request ID, UTC observation/completion timestamps, monotonic duration,
platform and capability objects, `status`, `data`, evidence, warnings, errors,
and explicit coverage. Status is one of `success`, `partial`, `unsupported`, or
`failure`. A zero is an observed zero; unavailable data is omitted or reported
as unsupported, never invented.

Linux CPU values are deltas between two `/proc/stat` samples. Guest time is not
double-counted because the kernel already includes it in user/nice counters.
Idle, I/O wait, steal, and guest percentages remain separate. Process CPU is
sampled by PID plus `/proc/PID/stat` start time; PID reuse, disappearance, and
counter reset invalidate the rate. One fully occupied logical CPU is 100%.
Network and disk values are counter deltas per monotonic second; resets and
interface churn are explicit. Disk throughput uses top-level leaf block devices
and 512-byte kernel sectors, avoiding partition/stack double counting. Capacity
and utilization are separate byte/percent fields.

## Workflows

- `diagnose-pressure` correlates bounded host and process samples, then separates
  observed facts, threshold-based hypotheses, missing evidence, and next checks.
- `investigate-file` combines a rooted file inspection, Antivermis rules,
  startup metadata, and (on Linux) matching running executable paths.
- `ai-health` combines host/process sampling with optional NVIDIA telemetry.
  Inference telemetry stays off unless `--allow-network`, an exact host, and a
  URL are supplied. It reports only metrics exported by that service.

These are decision-support reports, not autonomous remediation. They do not
kill processes, alter startup entries, install packages, quarantine files, or
claim causality from correlation.

## Trust and limits

Filesystem collectors accept only existing paths under explicit resolved roots.
Symlink escapes and special files are rejected. Reads, files, recursion,
responses, process rows, and execution time are bounded. Process environments,
credential stores, and arbitrary shell execution are excluded. Collected text
is untrusted evidence; strings that resemble prompts remain data and likely
secret assignments are redacted from previews.

Linux `/proc` and `/sys` collectors are unsupported on macOS instead of emitting
fake values. NVIDIA support requires `nvidia-smi`. The project is not an EDR,
malware sandbox, archive scanner, kernel-rootkit detector, RAG platform, or
hosted dashboard.

Future work may add signed signature manifests, archive scanning in a separate
resource sandbox, native macOS metric backends, and more GPU providers. Those
items are not implemented today.


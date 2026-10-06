# Security policy

Security fixes target the current `master` branch. Update to the latest reviewed
commit before relying on a fix; feature branches are development snapshots.

## Report a vulnerability

Use [GitHub's private vulnerability report](https://github.com/drmbios/pydev/security/advisories/new).
Include the affected commit, operating system, a minimal reproduction using
synthetic data, and the observed impact. Do not post real credentials, private
files, exploit targets, or malware samples in public issues.

## Operating boundaries

Run the tools with the least privileges needed. Antivermis reports indicators;
it cannot certify a clean host or detect every kernel rootkit. Scanning reads
regular files under work limits and does not unpack archives or execute samples.

MCP provides read-only collection under configured filesystem roots. Configure
small roots that exclude credentials. Preview redaction is heuristic, not a
guarantee that arbitrary secrets will be detected. The tools are not an operating
system sandbox: an attacker able to replace parent directories during traversal
may race pathname checks. Use an isolated account or read-only snapshot for
actively hostile, mutable trees, and do not run the MCP server as root.

Signature manifests rely on the configured HTTPS publisher and their advertised
SHA-256; they are not cryptographically signed. Offline `file://` updates should
use trusted local files. Do not treat a checksum as proof of publisher identity.

## Repository controls

GitHub CodeQL default setup scans C/C++, Python, and Actions. Dependabot checks
dependencies and action updates, while dependency review rejects newly introduced
high/critical advisories in pull requests. Actions are pinned to verified commit
IDs and receive only the permissions their job needs. Live settings and scan
results are visible in the repository Security and Actions pages; a workflow
file alone does not prove a scan has run or passed.

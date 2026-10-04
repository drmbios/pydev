# Security review — 2026-10-04

The public repository had no security policy. Authenticated inspection also
found secret scanning, push protection, CodeQL, automated security updates,
and private vulnerability reporting disabled, an unprotected default branch,
and write-enabled default Actions tokens.

## Applied repository settings

- Secret scanning and secret push protection enabled; the initial alert query
  returned no detected secrets. This is not proof that all secret formats were
  detected.
- Vulnerability alerts and Dependabot security updates enabled.
- CodeQL default setup enabled with extended queries for C/C++, Python, and
  Actions. Its initial analysis completed successfully and reported four alerts.
- Private vulnerability reporting enabled.
- Default Actions token reduced to read access; Actions cannot approve PRs.
- `master` requires a pull request, an up-to-date branch, passing `test` CI,
  and resolved conversations. Protection also applies to administrators;
  force pushes and deletion are disallowed. No second-person review is required
  because this is a single-maintainer repository.

## Initial CodeQL triage

| Alert | Finding | Review |
| --- | --- | --- |
| 1 | Game answer printed by `codebreaker.py` | Expected Mastermind behavior; not a credential. |
| 2 | C Antivermis pathname check/open race | Recursive child access now uses pinned directory descriptors and `openat`, with opened identity checks. |
| 3 | C SAR input pathname check/open race | Replaced with nonblocking open followed by descriptor validation and byte-bounded reads. |
| 4 | Login metadata printed by `sessionx` | Expected local administration output; no remote transfer or secret values. |

The configured GitHub token can read code-scanning alerts but GitHub rejected
alert dismissal with HTTP 403. Alerts 1 and 4 remain open pending maintainer
triage. Code fixes for 2 and 3 require review/merge and a default-branch rescan;
their existence on a feature branch does not close the default-branch alerts.

## Verification and limits

The local C functional suite and AddressSanitizer/UndefinedBehaviorSanitizer
suite passed, as did 33 Python tests including 13 new security regressions.
GitHub checks exercise the Linux-specific paths and MCP integration on the PR.
Leak detection availability depends on the sanitizer runtime; successful tests
are not a guarantee of no memory leaks, races, or undiscovered vulnerabilities.

Dependency updates and dependency review are configured in this branch and
take effect according to GitHub's default-branch workflow rules after merging.
See [SECURITY.md](../SECURITY.md) for operating limits, including the remaining
Python pathname-traversal race boundary and heuristic preview redaction.

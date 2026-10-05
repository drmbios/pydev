# Quantum-host and post-quantum toolkit

The five additions have independent C11 and Python 3.9+ editions. They run on
ordinary Linux and macOS CPUs. They do **not** access a physical quantum processor,
control laboratory hardware, provide quantum randomness, or make an operating
system quantum-safe. Existing commands remain available; `randpass` gains an
additive `--quantum` mode.

## New commands

| Command | Purpose | Limits / dependency |
| --- | --- | --- |
| `pqcheck` | Show backend version, KEMs and signatures | Local OpenSSL, 64 KiB output per invocation, 10-second deadline |
| `pqkey ALGORITHM NEW_DIRECTORY` | Generate a PEM key pair | OpenSSL 3.5+ default provider; new private directory only |
| `qasmcheck FILE` | Validate/count a restricted OpenQASM 2 circuit | 256 KiB file, 20 qubits, 2,048 gates |
| `qsim FILE` | Ideal computational-basis probabilities | Same input budget; 10 qubits, 1,024 output rows |
| `qbudget QUBITS` | Estimate complex128 statevector payload memory | 1–50 qubits; no state allocation |

Use `bin/NAME` for C and `python3 NAME.py` for Python. These commands are not
exposed by the read-only MCP server. Generating secrets is not a read-only collector.

## Post-quantum cryptography (PQC)

PQC runs on classical computers and is designed to resist known quantum attacks.
ML-KEM is a **key encapsulation mechanism**, not a file cipher. ML-DSA provides
digital signatures. Both are standardized by [NIST](https://www.nist.gov/pqc).
This toolkit delegates cryptography to OpenSSL, without implementing primitives.

```sh
make
bin/pqcheck
# Choose a new directory under a private parent, outside a source checkout:
bin/pqkey ML-KEM-768 /private/working/directory/pq-keys-kem
bin/pqkey ML-DSA-65 /private/working/directory/pq-keys-sign
```

Replace the example parent path with your own private directory. Supported
algorithms: `ML-KEM-768`, `ML-KEM-1024`, `ML-DSA-65`, `ML-DSA-87`.
OpenSSL 3.5+ supplies the primitives; actual provider availability is checked by
the operation. `pqcheck` is an inventory, not a yes/no security certification:
a successful inventory can still contain no PQ algorithms. `pqkey` fails if
the algorithm is unavailable, with **no classical fallback**. See OpenSSL's
[key generation](https://docs.openssl.org/3.5/man1/openssl-genpkey/) and
[key operations](https://docs.openssl.org/3.5/man1/openssl-pkeyutl/) documentation.

The macOS system executable may be LibreSSL. Select a trusted OpenSSL 3.5+
binary explicitly, for example on Apple Silicon with Homebrew:

```sh
export PYDEV_OPENSSL=/opt/homebrew/opt/openssl@3/bin/openssl
bin/pqcheck
```

The override selects one of four explicit allowlisted installations:
`/usr/bin/openssl` (default), `/usr/local/bin/openssl`,
`/opt/homebrew/opt/openssl@3/bin/openssl`, or `/usr/local/opt/openssl@3/bin/openssl`.
Arbitrary paths and `PATH` lookup are rejected. The selected installation and
its parent directories must be trusted. No shell is used. Config loading is disabled
(`OPENSSL_CONF=/dev/null`) and provider/engine path overrides are removed. Only
the default provider is requested; custom/FIPS-provider integration is not
implemented. This toolkit is not FIPS-validated.

Keys are `private.pem` and `public.pem`, mode 0600, in a new mode-0700 directory.
The private key is **unencrypted at rest**. Use a private, stable parent
directory; do not run as root or in an attacker-controlled directory. Protect
storage and backups, never commit keys, and use a reviewed key-management
system in production. Existing directories/files are never overwritten.
New partial files/directories remain on failure for inspection; do not use
incomplete key pairs. C buffers are cleared on cleanup. Python clears mutable
buffers but cannot guarantee removal of runtime copies. Neither edition
protects secrets from a compromised same-user process.

Tests cover key validation, ML-KEM encapsulation/decapsulation, and ML-DSA
signing/verification including tampered messages. Key generation alone does
not configure TLS, encrypt files, authenticate peers, or migrate applications
to PQC. Those require a reviewed protocol and key lifecycle.

### Password / secret mode

```sh
bin/randpass --quantum
python3 randpass.py --quantum
```

This prints 32 OS-CSPRNG bytes as exactly 64 lowercase hexadecimal characters:
256 bits of generated entropy, assuming a sound OS random source. The flag
names the requested high-entropy mode; it is **not** a quantum RNG, cipher,
OpenSSL key, or guarantee of 256-bit security against quantum attacks. The
receiving application must accept the full secret without truncation. Password
hashing, storage and protocol design still matter. Avoid logging the output.

## Circuit validation and ideal simulation

```sh
bin/qasmcheck examples/bell.qasm
bin/qsim examples/bell.qasm
python3 qsim.py examples/bell.qasm
bin/qbudget 30
```

The Bell example yields probabilities 0.5 for `00` and `11`. Qubit 0 is the least
significant bit; strings are `q[n-1]...q[0]`. The state begins at all-zero.
Every basis state is printed, rounded to 12 decimals; roundoff is expected.
There are no shots, noise, layout, pulse timing, or connectivity checks. Final
whole-register measurement has the same computational-basis distribution as
the premeasurement state; the simulator reports it without sampling/collapse.

This is a **restricted subset**, not a full
[OpenQASM 2 compiler](https://github.com/openqasm/openqasm/tree/OpenQASM2.x):

- Required prefix: `OPENQASM 2.0; include "qelib1.inc"; qreg q[N];`.
- One quantum register named `q`, with 1–20 qubits; simulation allows 1–10.
- Optional `creg c[N];` immediately after `qreg`, with matching size.
- Gates `h`, `x`, `z`, `s`, `t` on `q[index]`; `cx` / `cz` on distinct indices.
- Optional terminal `measure q -> c;`, with declared `c`. No gates afterward.
- ASCII whitespace and `//` comments. The include is a recognized built-in;
  no include file is opened or executed.
- Unsupported syntax, missing semicolons, custom gates, parameters, loops,
  reset, partial measurement, other register names and extra includes fail.

Depth counts dependency layers assuming unrelated gates can run in parallel,
not elapsed time or hardware-transpiled depth. `qbudget` computes
`16 * 2**qubits` bytes: thirty qubits need **16 GiB of payload alone**. It excludes
runtime overhead (especially Python objects), density matrices, noise, and
error correction. It estimates classical memory, not physical QPU resources.

## Selected existing tools for Linux quantum-control hosts

All existing tools were considered by role; this is the useful operational
subset. They inspect the **classical host**, not the quantum device.

| Tools | Useful role | Boundary |
| --- | --- | --- |
| `coreinfo`, `qbudget` | Host RAM/CPU inventory and simulation planning | Not QPU capability discovery |
| `syswatch`, `procexp` | Simulator CPU/RAM/I/O bottlenecks and ownership | Linux counters, not qubit utilization |
| `clockres` | OS clock resolution | Not scheduling latency or laboratory timing precision |
| `lsx`, `readjson`, `sql` | Experiment file sizes, results and notes | SQLite optional; no lab-system integration |
| `antivermis`, `autostartx` | Host threat indicators and startup inventory | Heuristics, not a malware-free certificate |
| `pydev_ai` file/system collectors | Bounded SHA-256 evidence and host snapshots | Allowlisted roots, no QPU telemetry |
| `randpass`, `pqcheck`, `pqkey` | Host secrets and PQ migration experiments | Not an end-to-end secure protocol |

`checksum` is CRC-32 for accidental corruption, **not** cryptographic artifact
authentication. Games, HTML/contact extractors, WHOIS, and number/syscall
references remain general-purpose; relabeling them quantum tools adds no
capability. `traceflow` and `sessionx` can disrupt experiments by tracing or
terminating control processes, so they are not routine live-laboratory tools.

## Verification

```sh
make check
make python-check
make sanitize
# Require real PQ interoperability, rather than allowing an unavailable-backend skip:
PYDEV_REQUIRE_PQ=1 python3 -m unittest discover -v -s tests -p test_quantum.py
```

Tests cover C/Python parity, Bell states, phase gates, control/target order,
normalization, malformed/oversized files, FIFO/symlink rejection, resource limits,
runner output flooding/timeouts, executable allowlisting, no-overwrite
behavior, permissions, and OpenSSL
interoperability. The Debian trixie CI job requires a working PQ backend.
Sanitizers and tests do not prove absence of leaks, denial-of-service paths,
cryptographic implementation flaws, or undiscovered bugs.

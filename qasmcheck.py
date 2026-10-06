"""Validate and count a restricted OpenQASM 2 circuit."""
import sys
from quantum import load, SIM_QUBITS
def main():
    try:
        if len(sys.argv) != 2:
            raise ValueError("usage: qasmcheck.py FILE")
        n, gates, depth = load(sys.argv[1])
        print(f"qubits={n} gates={len(gates)} depth={depth} simulator_supported={'yes' if n <= SIM_QUBITS else 'no'}")
        return 0
    except (OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
if __name__ == '__main__':
    raise SystemExit(main())

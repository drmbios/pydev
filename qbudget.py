"""Estimate complex128 statevector payload memory, without allocating it."""
import sys
from quantum import number
def main():
    try:
        if len(sys.argv) != 2:
            raise ValueError
        n = number(sys.argv[1],50)
        if not n:
            raise ValueError
        size = 16 << n
        print(f'qubits={n} amplitudes={1 << n} statevector_bytes={size} GiB={size / 1073741824:.9f}')
        print('Complex128 statevector only; excludes runtime overhead. Not a QPU resource estimate.')
        return 0
    except ValueError:
        print('usage: qbudget.py QUBITS:1-50', file=sys.stderr)
        return 2
if __name__ == '__main__':
    raise SystemExit(main())

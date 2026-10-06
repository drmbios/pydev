"""Inventory local OpenSSL providers; not proof of security or QPU access."""
import sys
import subprocess
from pqcommon import run
def main():
    try:
        if len(sys.argv) != 1:
            raise ValueError('usage: pqcheck.py')
        print('# Local backend inventory, not a security certification. Look for ML-KEM / ML-DSA.')
        for args in (['version'], ['list','-kem-algorithms','-provider','default'],
                     ['list','-signature-algorithms','-provider','default']):
            data = run(args)
            print(''.join(chr(c) if 32 <= c < 127 or c in (9,10) else '?' for c in data), end='')
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(str(exc), file=sys.stderr)
        return 2
if __name__ == '__main__':
    raise SystemExit(main())

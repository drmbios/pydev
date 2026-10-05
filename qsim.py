"""Small ideal statevector simulator; CPU only, no physical QPU backend."""
import math
import sys
from quantum import load, SIM_QUBITS

def simulate(n, gates):
    if not 1 <= n <= SIM_QUBITS or len(gates) > 2048:
        raise ValueError("simulation limit: 10 qubits, 2048 gates")
    state = [0j] * (1 << n)
    state[0] = 1+0j
    r = 1 / math.sqrt(2)
    for name, qa, qb in gates:
        a, b = 1 << qa, 1 << qb
        for i in range(len(state)):
            if name in ('cx', 'cz'):
                if i & a:
                    if name == 'cz':
                        if i & b:
                            state[i] = -state[i]
                    elif not i & b:
                        state[i], state[i|b] = state[i|b], state[i]
            elif name in ('h', 'x'):
                if not i & a:
                    u, v = state[i], state[i|a]
                    state[i], state[i|a] = ((u+v)*r, (u-v)*r) if name == 'h' else (v,u)
            elif i & a:
                state[i] *= {'z': -1, 's': 1j, 't': complex(r,r)}[name]
    return [v.real*v.real + v.imag*v.imag for v in state]

def main():
    try:
        if len(sys.argv) != 2:
            raise ValueError("usage: qsim.py FILE")
        n, gates, _ = load(sys.argv[1])
        probabilities = simulate(n,gates)
        print('# ideal probabilities; bit order q[n-1]..q[0]; no hardware/noise/shots')
        for i,p in enumerate(probabilities):
            print(f'{i:0{n}b} {p:.12f}')
        return 0
    except (OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
if __name__ == '__main__':
    raise SystemExit(main())

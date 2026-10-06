"""Bounded, deliberately restricted OpenQASM 2 parser (not a full compiler)."""
from __future__ import annotations
import re
from pycommon import read_bytes

MAX_QUBITS = 20
MAX_GATES = 2048
SIM_QUBITS = 10

def number(text: str, maximum: int) -> int:
    if not re.fullmatch(r"[0-9]{1,63}", text) or int(text) > maximum:
        raise ValueError("invalid or out-of-range integer")
    return int(text)

def load(path: str):
    text = read_bytes(path, 262144).decode("ascii")
    if "\0" in text:
        raise ValueError("NUL in input")
    tokens = [t for t in re.findall(r'//[^\n]*|"[^"\n\r]*"|[A-Za-z0-9_][A-Za-z0-9_.]*|[^ \t\n\r\f\v]', text)
              if not t.startswith('//')]
    if any(len(t) > 63 for t in tokens):
        raise ValueError("token too long")
    at = 0
    def take(wanted=None):
        nonlocal at
        if at == len(tokens) or (wanted is not None and tokens[at] != wanted):
            raise ValueError("invalid or unsupported QASM (see docs/QUANTUM.md)")
        token = tokens[at]
        at += 1
        return token
    for token in ('OPENQASM', '2.0', ';', 'include', '"qelib1.inc"', ';', 'qreg', 'q', '['):
        take(token)
    n = number(take(), MAX_QUBITS)
    if not n:
        raise ValueError("empty register")
    take(']'); take(';')
    creg = False
    if at < len(tokens) and tokens[at] == 'creg':
        take('creg'); take('c'); take('[')
        if number(take(), MAX_QUBITS) != n:
            raise ValueError("classical register must match q")
        take(']'); take(';'); creg = True
    def operand():
        take('q'); take('[')
        q = number(take(), n-1)
        take(']')
        return q
    gates, layers = [], [0]*n
    while at < len(tokens):
        name = take()
        if name == 'measure':
            if not creg:
                raise ValueError("missing classical register")
            for token in ('q', '-', '>', 'c', ';'):
                take(token)
            if at != len(tokens):
                raise ValueError("measurement must be terminal")
            break
        if name not in ('h', 'x', 'z', 's', 't', 'cx', 'cz') or len(gates) == MAX_GATES:
            raise ValueError("unsupported gate or gate budget exceeded")
        a, b = operand(), 0
        if name.startswith('c'):
            take(','); b = operand()
            if a == b:
                raise ValueError("control equals target")
        take(';')
        depth = 1 + max(layers[a], layers[b] if name.startswith('c') else 0)
        layers[a] = depth
        if name.startswith('c'):
            layers[b] = depth
        gates.append((name,a,b))
    return n, gates, max(layers)

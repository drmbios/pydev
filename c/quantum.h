#ifndef PYDEV_QUANTUM_H
#define PYDEV_QUANTUM_H
#define Q_MAX_QUBITS 20U
#define Q_MAX_GATES 2048U
#define Q_SIM_QUBITS 10U
typedef struct { char name[4]; unsigned a, b; } QGate;
typedef struct { unsigned qubits, count, depth; QGate gates[Q_MAX_GATES]; } QCircuit;
int q_load(const char *path, QCircuit *circuit);
int q_number(const char *text, unsigned maximum, unsigned *value);
#endif

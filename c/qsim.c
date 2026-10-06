#include "quantum.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    QCircuit c;
    double complex *state;
    size_t size;
    const double r = 1.0 / sqrt(2.0);
    if (argc != 2) { fputs("usage: qsim FILE\n",stderr); return 2; }
    if (q_load(argv[1],&c)) return 2;
    if (c.qubits > Q_SIM_QUBITS) { fputs("simulation limit: 10 qubits\n",stderr); return 2; }
    size = (size_t)1U << c.qubits;
    state = calloc(size,sizeof *state);
    if (!state) return 1;
    state[0] = 1;
    for (unsigned g=0;g<c.count;g++) {
        QGate gate = c.gates[g];
        size_t a = (size_t)1U << gate.a, b = (size_t)1U << gate.b;
        for (size_t i=0;i<size;i++) {
            if (gate.name[0] == 'c') {
                if (!(i & a)) continue;
                if (gate.name[1] == 'z') { if (i & b) state[i] = -state[i]; }
                else if (!(i & b)) { double complex tmp=state[i]; state[i]=state[i|b]; state[i|b]=tmp; }
            } else if (gate.name[0] == 'h' || gate.name[0] == 'x') {
                if (!(i & a)) {
                    double complex u=state[i], v=state[i|a];
                    if (gate.name[0]=='h') { state[i]=(u+v)*r; state[i|a]=(u-v)*r; }
                    else { state[i]=v; state[i|a]=u; }
                }
            } else if (i & a) {
                if (gate.name[0]=='z') state[i] = -state[i];
                else if (gate.name[0]=='s') state[i] *= I;
                else state[i] *= r + I*r;
            }
        }
    }
    puts("# ideal probabilities; bit order q[n-1]..q[0]; no hardware/noise/shots");
    for (size_t i=0;i<size;i++) {
        double p = creal(state[i])*creal(state[i]) + cimag(state[i])*cimag(state[i]);
        for (unsigned j=c.qubits;j>0;j--) putchar((i & ((size_t)1U << (j-1U))) ? '1' : '0');
        printf(" %.12f\n",p);
    }
    free(state);
    return ferror(stdout) ? 1 : 0;
}

#include "quantum.h"
#include <stdio.h>
int main(int argc, char **argv) {
    QCircuit circuit;
    if (argc != 2) { fputs("usage: qasmcheck FILE\n",stderr); return 2; }
    if (q_load(argv[1],&circuit)) return 2;
    printf("qubits=%u gates=%u depth=%u simulator_supported=%s\n",circuit.qubits,
        circuit.count,circuit.depth,circuit.qubits <= Q_SIM_QUBITS ? "yes" : "no");
    return ferror(stdout) ? 1 : 0;
}

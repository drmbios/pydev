#include "quantum.h"
#include <inttypes.h>
#include <stdio.h>
int main(int argc, char **argv) {
    unsigned n;
    uint64_t bytes;
    if (argc != 2 || q_number(argv[1],50U,&n) || !n) {
        fputs("usage: qbudget QUBITS:1-50\n",stderr); return 2;
    }
    bytes = UINT64_C(16) << n;
    printf("qubits=%u amplitudes=%" PRIu64 " statevector_bytes=%" PRIu64 " GiB=%.9f\n",
        n,UINT64_C(1)<<n,bytes,(double)bytes/1073741824.0);
    puts("Complex128 statevector only; excludes runtime overhead. Not a QPU resource estimate.");
    return ferror(stdout) ? 1 : 0;
}

#include "pqcommon.h"
#include <stdio.h>
int main(int argc, char **argv) {
    unsigned char data[PQ_OUTPUT]; size_t n;
    char *version[] = {"openssl","version",NULL};
    char *kem[] = {"openssl","list","-kem-algorithms","-provider","default",NULL};
    char *sig[] = {"openssl","list","-signature-algorithms","-provider","default",NULL};
    char **commands[] = {version,kem,sig};
    (void)argv;
    if (argc != 1) { fputs("usage: pqcheck\n",stderr); return 2; }
    puts("# Local backend inventory, not a security certification. Look for ML-KEM / ML-DSA.");
    for (unsigned i=0;i<3;i++) {
        if (pq_run(commands[i],-1,data,&n)) return 2;
        for (size_t j=0;j<n;j++) {
            unsigned char c = data[j];
            putchar((c >= 32 && c < 127) || c=='\n' || c=='\t' ? c : '?');
        }
    }
    return ferror(stdout) ? 1 : 0;
}

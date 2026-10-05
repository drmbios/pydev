#ifndef PYDEV_PQCOMMON_H
#define PYDEV_PQCOMMON_H
#include <stddef.h>
#define PQ_OUTPUT 65536U
int pq_run(char *const args[], int input, unsigned char *output, size_t *length);
void pq_clear(void *buffer, size_t length);
int pq_algorithm(const char *name);
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void binary(uint64_t value) {
    char bits[65];
    size_t index = sizeof bits;
    bits[--index] = '\0';
    do { bits[--index] = (char)('0' + (value & 1U)); value >>= 1U; } while (value != 0U);
    printf("binary:  %s\n", &bits[index]);
}

int main(int argc, char **argv) {
    char *end;
    uintmax_t value;
    if (argc != 2) { fprintf(stderr, "usage: %s NUMBER\n", argv[0]); return 2; }
    if (argv[1][0] == '-') { fputs("numconv: negative values are not supported\n", stderr); return 2; }
    errno = 0;
    value = strtoumax(argv[1], &end, 0);
    if (errno == ERANGE || end == argv[1] || *end != '\0' || value > UINT64_MAX) {
        fprintf(stderr, "numconv: invalid or overflowing integer: %s\n", argv[1]);
        return 2;
    }
    printf("decimal: %" PRIuMAX "\nhex:     0x%" PRIxMAX "\noctal:   0%" PRIoMAX "\n",
           value, value, value);
    binary((uint64_t)value);
    return 0;
}

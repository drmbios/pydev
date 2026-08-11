#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define BYTES_PER_LINE 16U

int main(int argc, char **argv) {
    char *data = NULL;
    size_t length = 0;
    size_t shown;
    size_t offset;
    long requested = (long)MAX_INPUT_SIZE;

    if (argc < 2 || argc > 3 ||
        (argc == 3 && parse_long(argv[2], 1, (long)MAX_INPUT_SIZE, &requested) != 0)) {
        fprintf(stderr, "usage: %s FILE [MAX_BYTES:1-%u]\n", argv[0], MAX_INPUT_SIZE);
        return 2;
    }
    if (read_file_bounded(argv[1], &data, &length) != 0) return 1;
    shown = length < (size_t)requested ? length : (size_t)requested;

    for (offset = 0; offset < shown; offset += BYTES_PER_LINE) {
        size_t column;
        printf("%08zx  ", offset);
        for (column = 0; column < BYTES_PER_LINE; ++column) {
            size_t position = offset + column;
            if (position < shown) printf("%02x ", (unsigned char)data[position]);
            else fputs("   ", stdout);
            if (column == 7) putchar(' ');
        }
        fputs(" |", stdout);
        for (column = 0; column < BYTES_PER_LINE && offset + column < shown; ++column) {
            unsigned char byte = (unsigned char)data[offset + column];
            putchar(isprint(byte) ? byte : '.');
        }
        puts("|");
    }
    if (shown < length) fprintf(stderr, "hexview: output limited to %zu of %zu bytes\n", shown, length);
    free(data);
    return 0;
}

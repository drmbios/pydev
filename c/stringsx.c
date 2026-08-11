#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_MINIMUM 4L
#define MAXIMUM_MINIMUM 4096L

int main(int argc, char **argv) {
    char *data = NULL;
    size_t length = 0;
    size_t start = 0;
    size_t index = 0;
    long minimum = DEFAULT_MINIMUM;

    if (argc < 2 || argc > 3 ||
        (argc == 3 && parse_long(argv[2], 1, MAXIMUM_MINIMUM, &minimum) != 0)) {
        fprintf(stderr, "usage: %s FILE [MIN_LENGTH:1-%ld]\n", argv[0], MAXIMUM_MINIMUM);
        return 2;
    }
    if (read_file_bounded(argv[1], &data, &length) != 0) return 1;

    while (index <= length) {
        if (index < length && isprint((unsigned char)data[index])) {
            ++index;
            continue;
        }
        if (index - start >= (size_t)minimum) {
            if (fwrite(data + start, 1, index - start, stdout) != index - start || putchar('\n') == EOF) {
                perror("stdout");
                free(data);
                return 1;
            }
        }
        ++index;
        start = index;
    }
    free(data);
    return 0;
}

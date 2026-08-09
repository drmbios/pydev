#include "common.h"
#include "jsonfmt.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char *text; size_t length; int result;
    if (argc != 2) { fprintf(stderr, "usage: %s FILE\n", argv[0]); return 2; }
    if (read_file_bounded(argv[1], &text, &length)) return 1;
    result = print_json(text, length, 0);
    free(text);
    return result ? 1 : 0;
}

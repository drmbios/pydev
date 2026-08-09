#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char *text;
    size_t length, i;
    unsigned long counts[26] = {0};
    if (argc != 2) { fprintf(stderr, "usage: %s FILE\n", argv[0]); return 2; }
    if (read_file_bounded(argv[1], &text, &length)) return 1;
    for (i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (isalpha(ch)) counts[tolower(ch) - 'a']++;
    }
    for (i = 0; i < 26; ++i) printf("%c %lu\n", (int)('a' + i), counts[i]);
    free(text);
    return 0;
}

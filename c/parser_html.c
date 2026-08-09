#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int equal_ci(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}

int main(int argc, char **argv) {
    char *html; size_t length, i = 0, found = 0;
    if (argc != 2) { fprintf(stderr, "usage: %s FILE\n", argv[0]); return 2; }
    if (read_file_bounded(argv[1], &html, &length)) return 1;
    while (i + 4 < length) {
        if (equal_ci(html + i, "href", 4)) {
            size_t p = i + 4, start; char quote;
            while (p < length && isspace((unsigned char)html[p])) p++;
            if (p >= length || html[p++] != '=') { i++; continue; }
            while (p < length && isspace((unsigned char)html[p])) p++;
            if (p >= length || (html[p] != '\'' && html[p] != '"')) { i++; continue; }
            quote = html[p++]; start = p;
            while (p < length && html[p] != quote) p++;
            if (p < length) { printf("%.*s\n", (int)(p - start), html + start); found++; i = p + 1; continue; }
        }
        i++;
    }
    free(html);
    return found ? 0 : 1;
}

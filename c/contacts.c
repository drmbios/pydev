#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int begins_ci(const char *line, size_t length, const char *word) {
    size_t i, n = strlen(word);
    if (length < n) return 0;
    for (i = 0; i < n; ++i)
        if (tolower((unsigned char)line[i]) != tolower((unsigned char)word[i])) return 0;
    return 1;
}

int main(int argc, char **argv) {
    char *text, *line, *next; size_t length, contacts = 0, phones = 0; int inside = 0;
    if (argc != 2) { fprintf(stderr, "usage: %s FILE.vcf\n", argv[0]); return 2; }
    if (read_file_bounded(argv[1], &text, &length)) return 1;
    (void)length;
    line = text;
    while (line && *line) {
        size_t n; next = strchr(line, '\n'); if (next) *next++ = '\0';
        n = strlen(line); if (n && line[n - 1] == '\r') line[--n] = '\0';
        if (begins_ci(line, n, "BEGIN:VCARD")) inside = 1;
        else if (inside && begins_ci(line, n, "END:VCARD")) { contacts++; inside = 0; }
        else if (inside && begins_ci(line, n, "FN:")) printf("%s\n", line + 3);
        else if (inside && begins_ci(line, n, "TEL")) {
            char *colon = strchr(line, ':'); if (colon) { printf("  %s\n", colon + 1); phones++; }
        }
        line = next;
    }
    printf("Total contacts: %zu; phone numbers: %zu\n", contacts, phones);
    free(text);
    return inside ? 1 : 0;
}

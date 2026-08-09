#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *file;
    size_t length;
    if (argc != 3) { fprintf(stderr, "usage: %s FILE TEXT\n", argv[0]); return 2; }
    length = strlen(argv[2]);
    if (length > 1024U * 1024U) { fputs("text exceeds 1 MiB limit\n", stderr); return 2; }
    file = fopen(argv[1], "wb");
    if (!file) { perror(argv[1]); return 1; }
    if (fwrite(argv[2], 1, length, file) != length || fclose(file) == EOF) {
        fprintf(stderr, "%s: write failed: %s\n", argv[1], strerror(errno));
        return 1;
    }
    printf("saved %zu bytes to %s\n", length, argv[1]);
    return 0;
}

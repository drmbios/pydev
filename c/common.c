#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int read_file_bounded(const char *path, char **data, size_t *length) {
    FILE *file = NULL;
    struct stat info;
    char *buffer = NULL;
    size_t used;

    if (!path || !data || !length || stat(path, &info) != 0) {
        perror(path ? path : "read_file_bounded");
        return -1;
    }
    if (info.st_size < 0 || (unsigned long long)info.st_size > MAX_INPUT_SIZE) {
        fprintf(stderr, "%s: input exceeds %u-byte limit\n", path, MAX_INPUT_SIZE);
        return -1;
    }
    file = fopen(path, "rb");
    if (!file) { perror(path); return -1; }
    buffer = malloc((size_t)info.st_size + 1U);
    if (!buffer) { perror("malloc"); fclose(file); return -1; }
    used = fread(buffer, 1, (size_t)info.st_size, file);
    if (ferror(file)) { perror(path); free(buffer); fclose(file); return -1; }
    buffer[used] = '\0';
    fclose(file);
    *data = buffer;
    *length = used;
    return 0;
}

int parse_long(const char *text, long minimum, long maximum, long *value) {
    char *end;
    long parsed;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        return -1;
    *value = parsed;
    return 0;
}

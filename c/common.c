#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define INITIAL_CAPACITY 4096U

int read_file_bounded(const char *path, char **data, size_t *length) {
    struct stat info;
    char *buffer = NULL;
    size_t capacity;
    size_t used = 0;
    int descriptor;

    if (!path || !data || !length) {
        errno = EINVAL;
        perror("read_file_bounded");
        return -1;
    }
    *data = NULL;
    *length = 0;
    descriptor = open(path, O_RDONLY | O_NONBLOCK);
    if (descriptor < 0) {
        perror(path);
        return -1;
    }
    (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    if (fstat(descriptor, &info) != 0) {
        perror(path);
        close(descriptor);
        return -1;
    }
    if (!S_ISREG(info.st_mode) && !S_ISCHR(info.st_mode)) {
        fprintf(stderr, "%s: unsupported input type (regular files only)\n", path);
        close(descriptor);
        return -1;
    }
    if (S_ISREG(info.st_mode) &&
        (info.st_size < 0 || (uintmax_t)info.st_size > (uintmax_t)MAX_INPUT_SIZE)) {
        fprintf(stderr, "%s: input exceeds %u-byte limit\n", path, MAX_INPUT_SIZE);
        close(descriptor);
        return -1;
    }
    capacity = S_ISREG(info.st_mode) && info.st_size > 0
        ? (size_t)info.st_size
        : INITIAL_CAPACITY;
    if (capacity > MAX_INPUT_SIZE) capacity = MAX_INPUT_SIZE;
    buffer = malloc(capacity + 1U);
    if (!buffer) {
        perror("malloc");
        close(descriptor);
        return -1;
    }

    for (;;) {
        ssize_t count;
        if (used == capacity) {
            if (capacity == MAX_INPUT_SIZE) {
                unsigned char extra;
                do count = read(descriptor, &extra, 1); while (count < 0 && errno == EINTR);
                if (count > 0) {
                    fprintf(stderr, "%s: input exceeds %u-byte limit\n", path, MAX_INPUT_SIZE);
                    free(buffer);
                    close(descriptor);
                    return -1;
                }
                if (count < 0) {
                    perror(path);
                    free(buffer);
                    close(descriptor);
                    return -1;
                }
                break;
            }
            {
                size_t grown_capacity = capacity > MAX_INPUT_SIZE / 2U
                    ? MAX_INPUT_SIZE
                    : capacity * 2U;
                char *grown = realloc(buffer, grown_capacity + 1U);
                if (!grown) {
                    perror("realloc");
                    free(buffer);
                    close(descriptor);
                    return -1;
                }
                buffer = grown;
                capacity = grown_capacity;
            }
        }
        do count = read(descriptor, buffer + used, capacity - used);
        while (count < 0 && errno == EINTR);
        if (count > 0) used += (size_t)count;
        else if (count == 0) break;
        else {
            perror(path);
            free(buffer);
            close(descriptor);
            return -1;
        }
    }
    if (close(descriptor) != 0) {
        perror(path);
        free(buffer);
        return -1;
    }
    buffer[used] = '\0';
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

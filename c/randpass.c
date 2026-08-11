#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_LENGTH 24L
#define MAX_PASSWORD_LENGTH 4096L

static const char alphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "abcdefghijkmnopqrstuvwxyz"
    "23456789"
    "!@#$%_-+=?";

static int fill_random(unsigned char *buffer, size_t length) {
    size_t offset = 0;
    int descriptor = open("/dev/urandom", O_RDONLY);
    if (descriptor < 0) {
        perror("/dev/urandom");
        return -1;
    }
    while (offset < length) {
        ssize_t count = read(descriptor, buffer + offset, length - offset);
        if (count > 0) offset += (size_t)count;
        else if (count < 0 && errno == EINTR) continue;
        else {
            if (count == 0) fputs("/dev/urandom: unexpected end of file\n", stderr);
            else perror("/dev/urandom");
            close(descriptor);
            return -1;
        }
    }
    if (close(descriptor) != 0) {
        perror("/dev/urandom");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    char password[MAX_PASSWORD_LENGTH + 1];
    unsigned char random_bytes[256];
    const size_t alphabet_length = sizeof alphabet - 1U;
    const unsigned rejection_limit = 256U - (256U % (unsigned)alphabet_length);
    long requested = DEFAULT_LENGTH;
    size_t produced = 0;

    if (argc > 2 ||
        (argc == 2 && parse_long(argv[1], 8, MAX_PASSWORD_LENGTH, &requested) != 0)) {
        fprintf(stderr, "usage: %s [LENGTH:8-%ld]\n", argv[0], MAX_PASSWORD_LENGTH);
        return 2;
    }
    while (produced < (size_t)requested) {
        size_t index;
        if (fill_random(random_bytes, sizeof random_bytes) != 0) return 1;
        for (index = 0; index < sizeof random_bytes && produced < (size_t)requested; ++index) {
            if ((unsigned)random_bytes[index] < rejection_limit) {
                password[produced++] = alphabet[random_bytes[index] % alphabet_length];
            }
        }
    }
    password[produced] = '\0';
    if (puts(password) == EOF) {
        perror("stdout");
        return 1;
    }
    return 0;
}

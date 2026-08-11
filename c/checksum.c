#include "common.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t crc32(const unsigned char *data, size_t length) {
    uint32_t crc = UINT32_C(0xffffffff);
    size_t i;

    for (i = 0; i < length; ++i) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & UINT32_C(1));
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return crc ^ UINT32_C(0xffffffff);
}

int main(int argc, char **argv) {
    int index;
    int status = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE...\n", argv[0]);
        return 2;
    }
    for (index = 1; index < argc; ++index) {
        char *data = NULL;
        size_t length = 0;
        if (read_file_bounded(argv[index], &data, &length) != 0) {
            status = 1;
            continue;
        }
        printf("%08" PRIx32 "  %s\n", crc32((const unsigned char *)data, length), argv[index]);
        free(data);
    }
    return status;
}

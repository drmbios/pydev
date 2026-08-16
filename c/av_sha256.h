#ifndef PYDEV_AV_SHA256_H
#define PYDEV_AV_SHA256_H

#include <stddef.h>
#include <stdint.h>

struct av_sha256 {
    uint32_t state[8];
    uint64_t bits;
    unsigned char block[64];
    size_t used;
};

void av_sha256_init(struct av_sha256 *context);
void av_sha256_update(struct av_sha256 *context, const unsigned char *data, size_t length);
void av_sha256_final(struct av_sha256 *context, unsigned char digest[32]);
void av_sha256_hex(const unsigned char digest[32], char output[65]);

#endif

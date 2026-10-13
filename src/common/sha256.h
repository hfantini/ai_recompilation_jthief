#ifndef JT_COMMON_SHA256_H
#define JT_COMMON_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t total_length;
    uint8_t block[64];
    size_t block_length;
} Sha256;

void sha256_init(Sha256 *ctx);
void sha256_update(Sha256 *ctx, const void *data, size_t length);
void sha256_final(Sha256 *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

void sha256_digest(const void *data, size_t length, uint8_t digest[SHA256_DIGEST_SIZE]);

#endif

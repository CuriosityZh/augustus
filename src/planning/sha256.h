#ifndef PLANNING_SHA256_H
#define PLANNING_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t state[8];
    uint64_t bytes;
    uint8_t block[64];
} plan_sha256;
void plan_sha256_init(plan_sha256* s);
void plan_sha256_update(plan_sha256* s, const void* bytes, size_t length);
void plan_sha256_finish(plan_sha256* s, uint8_t out[32]);
#endif

/* SHA-256, FIPS 180-4. Kept local so companion files do not add a dependency. */
#include "sha256.h"
#include <string.h>
static const uint32_t k[64]
    = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
          0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
          0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
          0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
          0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
          0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
          0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
          0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
static uint32_t ror(uint32_t v, unsigned n) { return v >> n | v << (32 - n); }
static void transform(plan_sha256* s, const uint8_t* p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8
            | p[4 * i + 3];
    for (int i = 16; i < 64; ++i) {
        uint32_t a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (ror(a, 7) ^ ror(a, 18) ^ (a >> 3)) + w[i - 7]
            + (ror(b, 17) ^ ror(b, 19) ^ (b >> 10));
    }
    uint32_t a = s->state[0], b = s->state[1], c = s->state[2], d = s->state[3];
    uint32_t e = s->state[4], f = s->state[5], g = s->state[6], h = s->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->state[0] += a;
    s->state[1] += b;
    s->state[2] += c;
    s->state[3] += d;
    s->state[4] += e;
    s->state[5] += f;
    s->state[6] += g;
    s->state[7] += h;
}
void plan_sha256_init(plan_sha256* s)
{
    static const uint32_t initial[8]
        = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->state, initial, sizeof(initial));
    s->bytes = 0;
}
void plan_sha256_update(plan_sha256* s, const void* bytes, size_t n)
{
    const uint8_t* p = bytes;
    while (n) {
        size_t at = s->bytes % 64, take = 64 - at;
        if (take > n)
            take = n;
        memcpy(s->block + at, p, take);
        s->bytes += take;
        p += take;
        n -= take;
        if (s->bytes % 64 == 0)
            transform(s, s->block);
    }
}
void plan_sha256_finish(plan_sha256* s, uint8_t out[32])
{
    uint64_t bits = s->bytes * 8;
    uint8_t one = 128, zero = 0;
    plan_sha256_update(s, &one, 1);
    while (s->bytes % 64 != 56)
        plan_sha256_update(s, &zero, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; ++i)
        len[7 - i] = (uint8_t)(bits >> (i * 8));
    plan_sha256_update(s, len, 8);
    for (int i = 0; i < 32; ++i)
        out[i] = (uint8_t)(s->state[i / 4] >> (24 - 8 * (i % 4)));
}

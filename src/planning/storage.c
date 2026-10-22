#include "storage.h"
#include "sha256.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#define HEADER_SIZE 88
#define RECORD_SIZE 52
static const uint8_t magic[8] = { 'A', 'U', 'G', 'P', 'L', 'A', 'N', 0 };
static void put32(uint8_t* b, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t* b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static void encode(const plan_object* p, uint8_t* b)
{
    uint32_t v[13] = { p->type, p->rotation, p->variant, p->bounds.x, p->bounds.y, p->bounds.w, p->bounds.h,
        p->secondary.x, p->secondary.y, p->secondary.w, p->secondary.h, p->bridge_direction,
        p->bridge_length };
    for (int i = 0; i < 13; ++i)
        put32(b + 4 * i, v[i]);
}
static void decode(plan_object* p, const uint8_t* b)
{
    p->type = get32(b);
    p->rotation = get32(b + 4);
    p->variant = get32(b + 8);
    p->bounds = (plan_rect) { (int32_t)get32(b + 12), (int32_t)get32(b + 16), (int32_t)get32(b + 20),
        (int32_t)get32(b + 24) };
    p->secondary = (plan_rect) { (int32_t)get32(b + 28), (int32_t)get32(b + 32), (int32_t)get32(b + 36),
        (int32_t)get32(b + 40) };
    p->bridge_direction = (int32_t)get32(b + 44);
    p->bridge_length = (int32_t)get32(b + 48);
}
static char* companion_name(const char* save, const char* suffix)
{
    if (!save || !save[0] || strlen(save) > 32700)
        return NULL;
    size_t n = strlen(save) + strlen(suffix) + 1;
    char* p = malloc(n);
    if (p)
        snprintf(p, n, "%s%s", save, suffix);
    return p;
}
static int hash_file(const char* name, uint8_t out[32])
{
    FILE* f = fopen(name, "rb");
    if (!f)
        return 0;
    uint8_t b[16384];
    size_t n;
    plan_sha256 s;
    plan_sha256_init(&s);
    while ((n = fread(b, 1, sizeof(b), f)))
        plan_sha256_update(&s, b, n);
    int ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    plan_sha256_finish(&s, out);
    return ok;
}
static int atomic_write(const char* name, const uint8_t* data, size_t n)
{
    char* temp = companion_name(name, ".tmp.XXXXXX");
    if (!temp)
        return 0;
#ifndef _WIN32
    int fd = mkstemp(temp);
    FILE* f = fd < 0 ? NULL : fdopen(fd, "wb");
    if (!f && fd >= 0) {
        close(fd);
        remove(temp);
    }
#else
    FILE* f = fopen(temp, "wb");
#endif
    if (!f) {
        free(temp);
        return 0;
    }
    int ok = fwrite(data, 1, n, f) == n;
    if (fflush(f))
        ok = 0;
#ifndef _WIN32
    if (ok && fsync(fileno(f)))
        ok = 0;
#endif
    if (fclose(f))
        ok = 0;
    if (ok && rename(temp, name))
        ok = 0;
    if (!ok)
        remove(temp);
    free(temp);
    return ok;
}
int plan_storage_save(const char* save, const plan_model* m, int w, int h, unsigned max_type)
{
    if (m->count > PLAN_CAPACITY)
        return 0;
    for (unsigned i = 0; i < m->count; ++i)
        if (!plan_object_valid(m->objects + i, w, h, max_type))
            return 0;
    size_t n = HEADER_SIZE + (size_t)m->count * RECORD_SIZE;
    uint8_t* b = calloc(1, n);
    if (!b)
        return 0;
    memcpy(b, magic, 8);
    put32(b + 8, 1);
    put32(b + 12, m->count);
    put32(b + 16, w);
    put32(b + 20, h);
    int ok = hash_file(save, b + 24);
    for (unsigned i = 0; i < m->count; ++i)
        encode(m->objects + i, b + HEADER_SIZE + i * RECORD_SIZE);
    plan_sha256 s;
    plan_sha256_init(&s);
    plan_sha256_update(&s, b, 56);
    plan_sha256_update(&s, b + HEADER_SIZE, n - HEADER_SIZE);
    plan_sha256_finish(&s, b + 56);
    char* name = companion_name(save, ".planning");
    if (ok && name)
        ok = atomic_write(name, b, n);
    else
        ok = 0;
    free(name);
    free(b);
    return ok;
}
int plan_storage_load(const char* save, plan_model* m, int w, int h, unsigned max_type)
{
    m->count = 0;
    char* name = companion_name(save, ".planning");
    if (!name)
        return -1;
    FILE* f = fopen(name, "rb");
    free(name);
    if (!f)
        return errno == ENOENT ? 0 : -1;
    uint8_t header[HEADER_SIZE];
    int ok = fread(header, 1, sizeof(header), f) == sizeof(header);
    unsigned count = ok ? get32(header + 12) : 0;
    if (!ok || memcmp(header, magic, 8) || get32(header + 8) != 1 || count > PLAN_CAPACITY
        || get32(header + 16) != (unsigned)w || get32(header + 20) != (unsigned)h) {
        fclose(f);
        return -1;
    }
    uint8_t save_hash[32];
    if (!hash_file(save, save_hash) || memcmp(save_hash, header + 24, 32)) {
        fclose(f);
        return -1;
    }
    plan_model* candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        fclose(f);
        return -1;
    }
    plan_sha256 s;
    plan_sha256_init(&s);
    plan_sha256_update(&s, header, 56);
    for (unsigned i = 0; i < count && ok; ++i) {
        uint8_t b[RECORD_SIZE];
        ok = fread(b, 1, sizeof(b), f) == sizeof(b);
        if (!ok)
            break;
        plan_sha256_update(&s, b, sizeof(b));
        decode(candidate->objects + i, b);
        ok = plan_object_valid(candidate->objects + i, w, h, max_type);
    }
    if (fgetc(f) != EOF || ferror(f))
        ok = 0;
    if (fclose(f))
        ok = 0;
    uint8_t digest[32];
    plan_sha256_finish(&s, digest);
    if (memcmp(digest, header + 56, 32))
        ok = 0;
    if (ok) {
        candidate->count = count;
        memcpy(m, candidate, sizeof(*m));
    }
    free(candidate);
    return ok ? 1 : -1;
}
int plan_storage_copy(const char* src, const char* dest)
{
    uint8_t a[32], b[32];
    if (!hash_file(src, a) || !hash_file(dest, b) || memcmp(a, b, 32))
        return 0;
    char *name = companion_name(src, ".planning"), *target = companion_name(dest, ".planning");
    FILE* f = name ? fopen(name, "rb") : NULL;
    free(name);
    if (!f) {
        if (target && errno == ENOENT)
            remove(target);
        free(target);
        return 0;
    }
    size_t cap = HEADER_SIZE + (size_t)PLAN_CAPACITY * RECORD_SIZE;
    uint8_t* data = malloc(cap);
    if (!data) {
        fclose(f);
        free(target);
        return 0;
    }
    size_t n = fread(data, 1, cap, f);
    int ok = !ferror(f) && fgetc(f) == EOF;
    if (fclose(f))
        ok = 0;
    if (ok && target)
        ok = atomic_write(target, data, n);
    else
        ok = 0;
    free(target);
    free(data);
    return ok;
}

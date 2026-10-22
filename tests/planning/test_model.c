#include "planning/model.h"
#include "planning/sha256.h"
#include "planning/storage.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static plan_model a, b;
static unsigned checks;
static void check_sha(const char* input, size_t n, const char* expected)
{
    uint8_t digest[32];
    char hex[65];
    plan_sha256 s;
    plan_sha256_init(&s);
    for (size_t i = 0; i < n; ++i)
        plan_sha256_update(&s, input + i, 1);
    plan_sha256_finish(&s, digest);
    for (int i = 0; i < 32; ++i)
        snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    CHECK(!strcmp(hex, expected));
    checks++;
}
static void write_file(const char* name, const void* data, size_t n)
{
    FILE* f = fopen(name, "wb");
    CHECK(f);
    CHECK(fwrite(data, 1, n, f) == n);
    CHECK(!fclose(f));
}
static size_t read_file(const char* name, uint8_t** out)
{
    FILE* f = fopen(name, "rb");
    CHECK(f);
    CHECK(!fseek(f, 0, SEEK_END));
    long n = ftell(f);
    CHECK(n >= 0);
    rewind(f);
    *out = malloc((size_t)n + 1);
    CHECK(*out);
    CHECK(fread(*out, 1, n, f) == (size_t)n);
    CHECK(!fclose(f));
    return n;
}
static void rehash(uint8_t* b, size_t n)
{
    plan_sha256 s;
    plan_sha256_init(&s);
    plan_sha256_update(&s, b, 56);
    plan_sha256_update(&s, b + 88, n - 88);
    plan_sha256_finish(&s, b + 56);
}
int main(void)
{
    check_sha("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check_sha("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    char* million = malloc(1000000);
    CHECK(million);
    memset(million, 'a', 1000000);
    check_sha(million, 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    free(million);
    plan_object p = { .type = 71, .bounds = { 10, 20, 3, 3 }, .rotation = 3, .variant = 7 };
    CHECK(plan_model_add(&a, &p, 100, 100, 300));
    CHECK(plan_model_add(&a, &p, 100, 100, 300));
    CHECK(a.count == 1);
    CHECK(plan_model_find(&a, 12, 22) == 0);
    CHECK(plan_model_find(&a, 13, 22) == -1);
    CHECK(!plan_model_move(&a, 0, INT_MAX, 0, 100, 100, 300));
    CHECK(!plan_model_move(&a, 0, 90, 0, 100, 100, 300));
    CHECK(a.objects[0].bounds.x == 10);
    CHECK(plan_model_move(&a, 0, 2, -2, 100, 100, 300));
    CHECK(a.objects[0].rotation == 3);
    uint8_t tiles[PLAN_CAPACITY] = { 0 };
    tiles[18 * 162 + 14] = 1;
    plan_model_remove_built(&a, tiles, 162);
    CHECK(!a.count);
    for (int type = 5; type <= 8; ++type)
        if (type != 7)
            for (int x = 0; x < 5; ++x) {
                p = (plan_object) { .type = type, .bounds = { x, type, 1, 1 } };
                CHECK(plan_model_add(&a, &p, 100, 100, 300));
            }
    memset(tiles, 0, sizeof(tiles));
    tiles[5 * 162 + 2] = tiles[6 * 162 + 2] = tiles[8 * 162 + 2] = 1;
    plan_model_remove_built(&a, tiles, 162);
    CHECK(a.count == 12);
    CHECK(plan_model_find(&a, 2, 5) == -1);
    CHECK(plan_model_find(&a, 3, 5) >= 0);
    p = (plan_object) { .type = 40, .bounds = { 20, 20, 3, 3 }, .secondary = { 23, 19, 4, 4 } };
    CHECK(plan_model_add(&a, &p, 100, 100, 300));
    CHECK(plan_model_find(&a, 26, 22) >= 0);
    CHECK(plan_model_move(&a, a.count - 1, 3, 4, 100, 100, 300));
    CHECK(a.objects[a.count - 1].secondary.x == 26);
    memset(tiles, 0, sizeof(tiles));
    tiles[23 * 162 + 26] = 1;
    plan_model_remove_built(&a, tiles, 162);
    CHECK(a.count == 12);
    checks += 16;
    p.bounds.x = INT_MAX;
    CHECK(!plan_object_valid(&p, 100, 100, 300));
    p.bounds.x = -1;
    CHECK(!plan_object_valid(&p, 100, 100, 300));
    char dir[] = "/tmp/augustus-planning-tests-XXXXXX";
    CHECK(mkdtemp(dir));
    CHECK(!chdir(dir));
    const char city[] = "immutable Augustus save fixture: not a real game";
    write_file("city.svx", city, sizeof(city));
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == 0);
    CHECK(!b.count);
    CHECK(plan_storage_save("city.svx", &a, 100, 100, 300));
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == 1);
    CHECK(a.count == b.count);
    CHECK(!memcmp(a.objects, b.objects, a.count * sizeof(plan_object)));
    uint8_t *original, *encoded;
    size_t original_n = read_file("city.svx", &original),
           encoded_n = read_file("city.svx.planning", &encoded);
    CHECK(original_n == sizeof(city) && !memcmp(original, city, sizeof(city)));
    /* Copy/rename works only with the exact save revision; filename is irrelevant. */
    write_file("renamed.svx", original, original_n);
    CHECK(plan_storage_copy("city.svx", "renamed.svx"));
    CHECK(plan_storage_load("renamed.svx", &b, 100, 100, 300) == 1);
    write_file("foreign.svx", "foreign", 7);
    write_file("foreign.svx.planning", encoded, encoded_n);
    CHECK(plan_storage_load("foreign.svx", &b, 100, 100, 300) == -1 && b.count == 0);
    CHECK(plan_storage_load("city.svx", &b, 99, 100, 300) == -1 && b.count == 0);
    /* Every truncation, including inside the header and final record, is rejected. */
    for (size_t n = 0; n < encoded_n; ++n) {
        write_file("city.svx.planning", encoded, n);
        CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == -1 && b.count == 0);
        checks++;
    }
    for (size_t i = 0; i < encoded_n; ++i) {
        encoded[i] ^= 0x80;
        write_file("city.svx.planning", encoded, encoded_n);
        CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == -1 && b.count == 0);
        encoded[i] ^= 0x80;
        checks++;
    }
    /* Valid checksum does not make malformed coordinates/counts acceptable. */
    uint8_t* bad = malloc(encoded_n);
    CHECK(bad);
    memcpy(bad, encoded, encoded_n);
    memset(bad + 88 + 12, 0xff, 4);
    rehash(bad, encoded_n);
    write_file("city.svx.planning", bad, encoded_n);
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == -1 && b.count == 0);
    memcpy(bad, encoded, encoded_n);
    memset(bad + 12, 0xff, 4);
    rehash(bad, encoded_n);
    write_file("city.svx.planning", bad, encoded_n);
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == -1 && b.count == 0);
    free(bad);
    write_file("city.svx.planning", encoded, encoded_n);
    FILE* f = fopen("city.svx.planning", "ab");
    CHECK(f);
    CHECK(fputc(0, f) != EOF);
    CHECK(!fclose(f));
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == -1 && b.count == 0);
    /* Atomic replacement failure never touches game bytes or the destination. */
    CHECK(!remove("city.svx.planning"));
    CHECK(!mkdir("city.svx.planning", 0700));
    CHECK(!plan_storage_save("city.svx", &a, 100, 100, 300));
    CHECK(!rmdir("city.svx.planning"));
    uint8_t* after;
    size_t after_n = read_file("city.svx", &after);
    CHECK(after_n == original_n && !memcmp(after, original, after_n));
    free(after);
    CHECK(!plan_storage_save("does-not-exist.svx", &a, 100, 100, 300));
    a.count = 0;
    CHECK(plan_storage_save("city.svx", &a, 100, 100, 300));
    CHECK(plan_storage_load("city.svx", &b, 100, 100, 300) == 1 && b.count == 0);
    free(encoded);
    free(original);
    checks += 20;
    puts("PASS: model geometry, multi-tile overlap, segment removal, movement, SHA-256 vectors, companion "
         "roundtrip/copy, corruption and write-failure isolation");
    printf("PASS: %u checks/corruption cases; fixture directory %s\n", checks, dir);
    return 0;
}

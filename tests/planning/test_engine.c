/* Reuse upstream's real SDL setup but provide a finite regression entrypoint.
 * The regular application contains neither this harness nor its fixture data. */
#define main planning_regular_main
#include "platform/SDL2/augustus.c"
#undef main
#include "building/construction.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "city/finance.h"
#include "city/view.h"
#include "core/file.h"
#include "game/file.h"
#include "game/file_io.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/renderer.h"
#include "graphics/screenshot.h"
#include "map/building.h"
#include "map/data.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "planning/planning.h"
#include "planning/sha256.h"
#include "planning/storage.h"
#include "platform/user_path.h"
#include "window/city.h"
#include <sys/stat.h>
#include <unistd.h>
#define VERIFY(x)                                                                                            \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "ENGINE FAIL line %d: %s\n", __LINE__, #x);                                      \
            exit(3);                                                                                         \
        }                                                                                                    \
    } while (0)
void planning_test_snapshot(uint8_t digest[32]);
static plan_model test_model;
static char scratch[1024];
static void test_hash(const char* file, uint8_t hash[32])
{
    FILE* f = fopen(file, "rb");
    VERIFY(f);
    plan_sha256 s;
    plan_sha256_init(&s);
    uint8_t bytes[16384];
    size_t n;
    while ((n = fread(bytes, 1, sizeof(bytes), f)))
        plan_sha256_update(&s, bytes, n);
    VERIFY(!ferror(f));
    fclose(f);
    plan_sha256_finish(&s, hash);
}
static void test_path(const char* name, char out[1200]) { snprintf(out, 1200, "%s/%s", scratch, name); }
static unsigned test_count(void)
{
    char p[1200];
    test_path("probe.svx", p);
    VERIFY(game_file_write_saved_game(p));
    VERIFY(plan_storage_load(p, &test_model, map_data.width, map_data.height, BUILDING_TYPE_MAX) == 1);
    return test_model.count;
}
static void test_click(int x, int y)
{
    map_tile tile = { x, y, map_grid_offset(x, y) };
    mouse m = { .x = -100, .y = -100 };
    m.left.went_down = 1;
    m.left.is_down = 1;
    VERIFY(planning_mouse(&m, &tile));
    m.left.went_down = 0;
    m.left.is_down = 0;
    m.left.went_up = 1;
    VERIFY(planning_mouse(&m, &tile));
}
static void test_drag(int x, int y, int ex, int ey)
{
    mouse m = { .x = -100, .y = -100 };
    map_tile t = { x, y, map_grid_offset(x, y) };
    m.left.went_down = 1;
    m.left.is_down = 1;
    VERIFY(planning_mouse(&m, &t));
    t = (map_tile) { ex, ey, map_grid_offset(ex, ey) };
    m.left.went_down = 0;
    m.left.is_down = 0;
    m.left.went_up = 1;
    VERIFY(planning_mouse(&m, &t));
}
static void test_real(int type, int x, int y)
{
    VERIFY(!planning_enabled());
    building_construction_set_type(type, 0);
    int g = map_grid_offset(x, y);
    building_construction_start(x, y, g);
    building_construction_update(x, y, g);
    building_construction_place();
    building_construction_clear_type();
}
static void test_toggle(void) { VERIFY(planning_key(KEY_TYPE_P, KEY_MOD_ALT, 0)); }
static void test_save(const char* name, uint8_t hash[32])
{
    char p[1200];
    test_path(name, p);
    VERIFY(game_file_write_saved_game(p));
    planning_test_snapshot(hash);
}
static void test_screenshot(const char* name)
{
    game_draw();
    graphics_save_screenshot(SCREENSHOT_DISPLAY);
    printf("RENDER %s\n", name);
}
int main(int argc, char** argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: planning-engine-tests C3_DATA_DIR ABSOLUTE_SCRATCH_DIR COPIED_SAVE\n");
        return 2;
    }
    snprintf(scratch, sizeof(scratch), "%s", argv[2]);
    VERIFY(scratch[0] == '/');
    VERIFY(!setenv("AUGUSTUS_PLANNING_USER_DIR", scratch, 1));
    augustus_args args = { .data_directory = argv[1], .force_windowed = 1 };
    setup(&args);
    platform_user_path_create_subdirectories();
    VERIFY(game_file_load_saved_game(argv[3]) == 1);
    game_state_pause();
    city_finance_treasury_add(100000);
    city_view_reset_orientation();
    window_city_show();
    /* Find a vacant district for actual construction tests, without clearing
     * anything in the fixture. All writes are confined to the scratch copy. */
    int sx = -1, sy = -1;
    for (int y = 5; y < map_data.height - 20 && sx < 0; ++y)
        for (int x = 5; x < map_data.width - 20; ++x) {
            int clear = 1;
            for (int yy = 0; yy < 16 && clear; ++yy)
                for (int xx = 0; xx < 16; ++xx)
                    if (map_terrain_is(map_grid_offset(x + xx, y + yy),
                            TERRAIN_BUILDING | TERRAIN_WATER | TERRAIN_ROCK | TERRAIN_ELEVATION
                                | TERRAIN_ACCESS_RAMP)) {
                        clear = 0;
                        break;
                    }
            if (clear) {
                sx = x;
                sy = y;
                break;
            }
        }
    VERIFY(sx >= 0);
    /* Clear vegetation only in this scratch test district before taking the
     * baseline. Never run a tick or write to the input fixture. */
    for (int y = sy; y < sy + 16; ++y)
        for (int x = sx; x < sx + 16; ++x)
            map_terrain_remove(map_grid_offset(x, y),
                TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD | TERRAIN_GARDEN | TERRAIN_AQUEDUCT | TERRAIN_WALL
                    | TERRAIN_RUBBLE);
    map_tiles_update_all_empty_land();
    city_view_go_to_grid_offset(map_grid_offset(sx + 7, sy + 7));
    uint8_t before[32], after[32];
    test_save("before.svx", before);
    test_save("control.svx", after);
    VERIFY(!memcmp(before, after, 32));
    test_toggle();
    building_construction_set_type(BUILDING_WAREHOUSE, 0);
    test_click(sx, sy);
    building_construction_set_type(BUILDING_WHEAT_FARM, 0);
    test_click(sx + 5, sy);
    building_construction_set_type(BUILDING_LARGE_TEMPLE_CERES, 0);
    test_click(sx + 10, sy);
    building_construction_set_type(BUILDING_ROAD, 0);
    test_drag(sx, sy + 7, sx + 4, sy + 7);
    building_construction_set_type(BUILDING_WALL, 0);
    test_drag(sx, sy + 9, sx + 4, sy + 9);
    building_construction_set_type(BUILDING_AQUEDUCT, 0);
    test_drag(sx, sy + 11, sx + 4, sy + 11);
    VERIFY(test_count() == 18);
    test_save("planned.svx", after);
    VERIFY(!memcmp(before, after, 32));
    /* Public construction entrypoints must also be harmless if invoked by
     * another input path while Planning Mode is active. */
    building_construction_start(sx, sy, map_grid_offset(sx, sy));
    building_construction_update(sx, sy, map_grid_offset(sx, sy));
    building_construction_place();
    test_save("guarded.svx", after);
    VERIFY(!memcmp(before, after, 32));
    building_construction_clear_type();
    test_toggle();
    test_save("off.svx", after);
    VERIFY(!memcmp(before, after, 32));
    planning_key(KEY_TYPE_H, KEY_MOD_ALT, 0);
    test_save("hidden.svx", after);
    VERIFY(!memcmp(before, after, 32));
    planning_key(KEY_TYPE_H, KEY_MOD_ALT, 0);
    printf("PASS ENGINE: planning, rendering preparation, visibility and exit leave simulation state "
           "unchanged\n");
    for (int r = 0; r < 4; ++r) {
        for (int z = 75; z <= 150; z += 75) {
            city_view_set_scale(z);
            planning_prepare_draw();
            test_screenshot("rotation and zoom");
        }
        city_view_rotate_right();
    }
    city_view_reset_orientation();
    city_view_set_scale(100);
    test_real(BUILDING_WELL, sx + 2, sy + 2);
    VERIFY(map_building_at(map_grid_offset(sx + 2, sy + 2)));
    VERIFY(test_count() == 17);
    test_real(BUILDING_ROAD, sx + 2, sy + 7);
    VERIFY(test_count() == 16);
    test_real(BUILDING_ROAD, sx + 2, sy + 9);
    VERIFY(test_count() == 15);
    test_real(BUILDING_ROAD, sx + 2, sy + 11);
    VERIFY(test_count() == 14);
    printf("PASS ENGINE: successful partial overlap deletes whole warehouse and only intersected "
           "road/wall/aqueduct segments\n");
    test_toggle();
    building_construction_set_type(BUILDING_WAREHOUSE, 0);
    test_click(sx, sy);
    VERIFY(test_count() == 15);
    test_toggle();
    test_real(BUILDING_WAREHOUSE, sx, sy);
    VERIFY(test_count() == 15);
    printf("PASS ENGINE: failed real construction retains plans\n");
    test_toggle();
    planning_key(KEY_TYPE_M, KEY_MOD_ALT, 0);
    test_click(sx + 5, sy);
    test_click(sx + 6, sy + 1);
    VERIFY(test_count() == 15);
    VERIFY(plan_model_find(&test_model, sx + 5, sy) == -1);
    VERIFY(plan_model_find(&test_model, sx + 8, sy + 3) >= 0);
    planning_undo();
    VERIFY(test_count() == 15);
    VERIFY(plan_model_find(&test_model, sx + 5, sy) >= 0);
    planning_key(KEY_TYPE_E, KEY_MOD_ALT, 0);
    test_click(sx + 5, sy);
    VERIFY(test_count() == 14);
    planning_undo();
    VERIFY(test_count() == 15);
    printf("PASS ENGINE: move, erase, undo preserve plan count and geometry\n");
    char saved[1200];
    test_path("probe.svx", saved);
    VERIFY(game_file_load_saved_game(saved) == 1);
    VERIFY(test_count() == 15);
    VERIFY(!planning_enabled());
    char companion[1400];
    snprintf(companion, sizeof(companion), "%s.planning", saved);
    VERIFY(!remove(companion));
    VERIFY(game_file_load_saved_game(saved) == 1);
    VERIFY(test_count() == 0);
    FILE* bad = fopen(companion, "wb");
    VERIFY(bad);
    fputs("broken", bad);
    fclose(bad);
    VERIFY(game_file_load_saved_game(saved) == 1);
    VERIFY(test_count() == 0);
    printf("PASS ENGINE: real city reloads with valid, missing and corrupt companion files\n");
    /* A bridge can be planned without touching the bridge cache or city. */
    planning_reset();
    test_toggle();
    building_construction_set_type(BUILDING_LOW_BRIDGE, 0);
    unsigned bridge_count = 0;
    for (int y = 1; y < map_data.height - 1 && !bridge_count; ++y)
        for (int x = 1; x < map_data.width - 1 && !bridge_count; ++x) {
            if (!map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER))
                continue;
            test_click(x, y);
            bridge_count = test_count();
        }
    VERIFY(bridge_count == 1);
    test_toggle();
    printf("PASS ENGINE: low bridge shoreline planning\n");
    puts("PASS ENGINE: all checks complete; only scratch saves were written");
    fflush(stdout);
    teardown();
    return 0;
}

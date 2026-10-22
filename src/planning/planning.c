#include "planning.h"
#include "assets/assets.h"
#include "building/building.h"
#include "building/construction.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/image.h"
#include "core/log.h"
#include "core/string.h"
#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/hotkey.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/data.h"
#include "map/image_context.h"
#include "map/terrain.h"
#include "map/water.h"
#include "model.h"
#include "storage.h"
#include "widget/city/bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CELLS (GRID_SIZE * GRID_SIZE)
#define PIECES (PLAN_CAPACITY * 9)
#define GHOST_COLOR 0x8797dfff
#define PREVIEW_COLOR 0xb7b5eeff
#define INVALID_COLOR 0x97ff8878
/* No game building, terrain, finance, routing, figure or random state is owned
 * or written here. Pieces are an independent render cache indexed by draw tile. */
typedef struct {
    int next, image, size, bridge_sprite, extra_image, extra_x, extra_y;
} plan_piece;
static struct {
    plan_model model, undo;
    int enabled, visible, tool, dragging, start_x, start_y, end_x, end_y;
    int selected, grabbed_x, grabbed_y, dirty, orientation, undo_valid;
    uint32_t before_terrain[CELLS], before_building[CELLS];
    int snapshot_valid;
    uint8_t built[CELLS], road[CELLS], wall[CELLS], aqueduct[CELLS];
    int heads[CELLS], piece_count;
    plan_piece pieces[PIECES];
    const char* status;
} data = { .visible = 1, .selected = -1, .dirty = 1, .orientation = -1 };
static int preview_drawing, preview_top, preview_x, preview_y, preview_offset;
static color_t preview_color;
static void invalidate(void)
{
    data.dirty = 1;
    window_invalidate();
}
static int inside(int x, int y) { return x >= 0 && y >= 0 && x < map_data.width && y < map_data.height; }
static int canonical(int x, int y) { return y * GRID_SIZE + x; }
static int is_fort(int type)
{
    return type == BUILDING_FORT_LEGIONARIES || type == BUILDING_FORT_JAVELIN || type == BUILDING_FORT_MOUNTED
        || type == BUILDING_FORT_AUXILIA_INFANTRY || type == BUILDING_FORT_ARCHERS;
}
static int supported(int type)
{
    return type > 0 && type <= BUILDING_TYPE_MAX && type != BUILDING_CLEAR_LAND
        && type != BUILDING_REPAIR_LAND
        && (type == BUILDING_DRAGGABLE_RESERVOIR || building_properties_for_type(type)->size > 0);
}
static void remember(void)
{
    data.undo = data.model;
    data.undo_valid = 1;
}
void planning_undo(void)
{
    if (!data.undo_valid)
        return;
    plan_model* temp = malloc(sizeof(*temp));
    if (!temp)
        return;
    *temp = data.model;
    data.model = data.undo;
    data.undo = *temp;
    free(temp);
    data.selected = -1;
    data.dragging = 0;
    invalidate();
}
void planning_reset(void)
{
    data.model.count = 0;
    data.undo_valid = 0;
    data.snapshot_valid = 0;
    data.enabled = 0;
    data.visible = 1;
    data.tool = 0;
    data.dragging = 0;
    data.selected = -1;
    data.status = 0;
    data.dirty = 1;
    data.orientation = -1;
}
int planning_enabled(void) { return data.enabled; }
static void toggle(void)
{
    /* Cancel an ordinary preview before switching; never leave its temporary
     * terrain or undo transaction active while planning. */
    if (!data.enabled && building_construction_type())
        building_construction_cancel();
    data.enabled = !data.enabled;
    data.visible = 1;
    data.tool = 0;
    data.dragging = 0;
    data.selected = -1;
    invalidate();
}
int planning_key(key_type key, key_modifier_type mods, int repeat)
{
    if (mods != KEY_MOD_ALT)
        return 0;
    if (key != KEY_TYPE_P && key != KEY_TYPE_H && key != KEY_TYPE_M && key != KEY_TYPE_E)
        return 0;
    if (repeat)
        return 1;
    if (key == KEY_TYPE_P)
        toggle();
    else if (key == KEY_TYPE_H) {
        data.visible = !data.visible;
        invalidate();
    } else if (data.enabled) {
        data.tool = key == KEY_TYPE_M ? 1 : 2;
        data.selected = -1;
        data.dragging = 0;
        invalidate();
    }
    return 1;
}
static void control_origin(int* x, int* y)
{
    int w, h;
    city_view_get_viewport(x, y, &w, &h);
    *x += 8;
    *y += h - 36;
}
void planning_draw_controls(void)
{
    int x, y;
    control_origin(&x, &y);
    const char* labels[4] = { data.enabled ? "Plan: ON" : "Plan: OFF",
        data.visible ? "Show plans" : "Plans hidden", "Move", "Erase" };
    int n = data.enabled ? 4 : 2;
    for (int i = 0; i < n; ++i) {
        inner_panel_draw(x + i * 96, y, 6, 2);
        button_border_draw(x + i * 96, y, 94, 32,
            (i == 0 && data.enabled) || (i == 2 && data.tool == 1) || (i == 3 && data.tool == 2));
        text_draw(string_from_ascii(labels[i]), x + i * 96 + 6, y + 9, FONT_SMALL_PLAIN, COLOR_FONT_YELLOW);
    }
    if (data.enabled || data.status) {
        const char* hint = data.status
            ? data.status
            : (data.tool == 1 ? "Move: click a plan, then its destination. Right click cancels."
                      : data.tool == 2
                      ? "Erase: drag across plans. Real buildings are never removed."
                      : "Planning: choose a building. Drag roads/walls/aqueducts. Alt+P exits.");
        text_draw(string_from_ascii(hint), x, y - 17, FONT_SMALL_PLAIN, COLOR_FONT_YELLOW);
    }
}
static int handle_controls(const mouse* m)
{
    int x, y;
    control_origin(&x, &y);
    int n = data.enabled ? 4 : 2;
    if (m->x < x || m->x >= x + 96 * n || m->y < y || m->y >= y + 32)
        return 0;
    if (m->left.went_up) {
        int i = (m->x - x) / 96;
        if (i == 0)
            toggle();
        else if (i == 1) {
            data.visible = !data.visible;
            invalidate();
        } else {
            data.tool = i == 2 ? 1 : 2;
            data.selected = -1;
            data.dragging = 0;
            invalidate();
        }
    }
    return 1;
}
static int make_object(int type, int x, int y, plan_object* p)
{
    memset(p, 0, sizeof(*p));
    if (!supported(type))
        return 0;
    p->type = type;
    p->rotation = building_rotation_get_rotation() % 4;
    int variants = building_variant_get_number_of_variants(type);
    p->variant = variants > 0 ? building_rotation_get_rotation_with_limit(variants) : 0;
    int size = type == BUILDING_WAREHOUSE || type == BUILDING_DRAGGABLE_RESERVOIR
        ? 3
        : building_properties_for_type(type)->size;
    if (type == BUILDING_DRAGGABLE_RESERVOIR) {
        p->type = BUILDING_RESERVOIR;
        x--;
        y--;
    } else
        building_construction_offset_start_from_orientation(&x, &y, size);
    p->bounds = (plan_rect) { x, y, size, size };
    if (type == BUILDING_GATEHOUSE || type == BUILDING_TRIUMPHAL_ARCH)
        p->rotation = building_rotation_get_road_orientation() - 1;
    if (is_fort(type)) {
        static const int dx[4] = { 3, -1, -4, 0 }, dy[4] = { -1, -4, 0, 3 };
        p->secondary = (plan_rect) { x + dx[p->rotation], y + dy[p->rotation], 4, 4 };
    }
    if (type == BUILDING_HIPPODROME) {
        if (p->rotation % 2) {
            p->bounds.h = 15;
            if (p->rotation == 1)
                p->bounds.y -= 10;
        } else {
            p->bounds.w = 15;
            if (p->rotation == 2)
                p->bounds.x -= 10;
        }
    }
    if (type == BUILDING_DOCK || type == BUILDING_WHARF || type == BUILDING_SHIPYARD) {
        int absolute = 0, relative = 0, blocked[25];
        map_water_determine_orientation(x, y, size, 0, &absolute, &relative, 0, blocked);
        p->rotation = absolute % 4;
    }
    if (building_type_is_bridge(type)) {
        /* Pure shoreline measurement: unlike the ordinary bridge preview this
         * does not overwrite the game's bridge construction cache. */
        static const int dx[4] = { 0, 1, 0, -1 }, dy[4] = { -1, 0, 1, 0 };
        if (!inside(x, y) || !map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER))
            return 0;
        int shore = -1, count = 0;
        for (int d = 0; d < 4; ++d)
            if (inside(x + dx[d], y + dy[d])
                && !map_terrain_is(map_grid_offset(x + dx[d], y + dy[d]), TERRAIN_WATER)) {
                shore = d;
                count++;
            }
        if (count != 1)
            return 0;
        int dir = (shore + 2) % 4, length = 1, xx = x, yy = y;
        while (length < 65 && inside(xx + dx[dir], yy + dy[dir])
            && map_terrain_is(map_grid_offset(xx + dx[dir], yy + dy[dir]), TERRAIN_WATER)) {
            xx += dx[dir];
            yy += dy[dir];
            length++;
        }
        if (!inside(xx + dx[dir], yy + dy[dir]) || length < 2 || length >= 65
            || (type == BUILDING_SHIP_BRIDGE && length < 5))
            return 0;
        p->bridge_direction = dir * 2;
        p->bridge_length = length;
        p->bounds = (plan_rect) { xx < x ? xx : x, yy < y ? yy : y, abs(xx - x) + 1, abs(yy - y) + 1 };
    }
    return plan_object_valid(p, map_data.width, map_data.height, BUILDING_TYPE_MAX);
}
static int network(int type)
{
    return type == BUILDING_ROAD || type == BUILDING_WALL || type == BUILDING_AQUEDUCT
        || type == BUILDING_HIGHWAY;
}
static int add_plan(plan_object* p)
{
    return plan_model_add(&data.model, p, map_data.width, map_data.height, BUILDING_TYPE_MAX);
}
static void emit_object(const plan_object* p);
static void place_one(int type, int x, int y, int commit)
{
    plan_object p;
    if (!make_object(type, x, y, &p))
        return;
    if (commit) {
        if (!add_plan(&p))
            data.status = "Plan limit reached; erase some plans before adding more.";
    } else
        emit_object(&p);
}
static void generate(int commit)
{
    int type = building_construction_type();
    if (!supported(type))
        return;
    int sx = data.dragging ? data.start_x : data.end_x, sy = data.dragging ? data.start_y : data.end_y;
    int ex = data.end_x, ey = data.end_y;
    if (network(type)) {
        /* Deliberate Manhattan routing rather than simulation routing. Shift
         * chooses the other elbow; terrain never becomes occupied by a plan. */
        int vertical_first = (hotkey_get_modifiers() & KEY_MOD_SHIFT) != 0;
        int x = sx, y = sy, step = type == BUILDING_HIGHWAY ? 2 : 1;
        place_one(type, x, y, commit);
        for (int axis = 0; axis < 2; ++axis) {
            int vertical = axis == 0 ? vertical_first : !vertical_first;
            int *v = vertical ? &y : &x, target = vertical ? ey : ex;
            while (abs(target - *v) >= step) {
                *v += target > *v ? step : -step;
                place_one(type, x, y, commit);
            }
        }
    } else if (building_construction_is_updatable() && type != BUILDING_DRAGGABLE_RESERVOIR) {
        for (int y = sy < ey ? sy : ey; y <= (sy > ey ? sy : ey); ++y)
            for (int x = sx < ex ? sx : ex; x <= (sx > ex ? sx : ex); ++x)
                place_one(type, x, y, commit);
    } else if (type == BUILDING_DRAGGABLE_RESERVOIR && data.dragging && (sx != ex || sy != ey)) {
        place_one(type, sx, sy, commit);
        place_one(type, ex, ey, commit);
        int x = sx, y = sy;
        while (x != ex) {
            if (abs(x - sx) > 1 && abs(x - ex) > 1)
                place_one(BUILDING_AQUEDUCT, x, y, commit);
            x += ex > x ? 1 : -1;
        }
        while (y != ey) {
            if (abs(y - ey) > 1)
                place_one(BUILDING_AQUEDUCT, x, y, commit);
            y += ey > y ? 1 : -1;
        }
    } else
        place_one(type, ex, ey, commit);
}
int planning_mouse(const mouse* m, const map_tile* tile)
{
    if (handle_controls(m))
        return 1;
    if (!data.enabled)
        return 0;
    if (tile->grid_offset && inside(tile->x, tile->y)) {
        data.end_x = tile->x;
        data.end_y = tile->y;
    }
    if (m->right.went_up) {
        if (data.selected >= 0 || data.dragging) {
            data.selected = -1;
            data.dragging = 0;
        } else {
            data.tool = 0;
            building_construction_clear_type();
        }
        invalidate();
        return 1;
    }
    if (!tile->grid_offset || !inside(tile->x, tile->y)) {
        if (m->left.went_up)
            data.dragging = 0;
        return 1;
    }
    if (data.tool == 1) {
        if (m->left.went_up) {
            if (data.selected < 0) {
                data.selected = plan_model_find(&data.model, tile->x, tile->y);
                data.grabbed_x = tile->x;
                data.grabbed_y = tile->y;
            } else {
                remember();
                if (plan_model_move(&data.model, data.selected, tile->x - data.grabbed_x,
                        tile->y - data.grabbed_y, map_data.width, map_data.height, BUILDING_TYPE_MAX)) {
                    data.selected = -1;
                    data.status = 0;
                } else
                    data.status = "The entire plan must fit inside the map.";
            }
            invalidate();
        }
        return 1;
    }
    if (m->left.went_down) {
        data.start_x = tile->x;
        data.start_y = tile->y;
        data.dragging = 1;
        data.status = 0;
    }
    if (m->left.went_up && data.dragging) {
        remember();
        if (data.tool == 2 || building_construction_type() == BUILDING_CLEAR_LAND) {
            memset(data.built, 0, sizeof(data.built));
            for (int y = data.start_y < tile->y ? data.start_y : tile->y;
                y <= (data.start_y > tile->y ? data.start_y : tile->y); ++y)
                for (int x = data.start_x < tile->x ? data.start_x : tile->x;
                    x <= (data.start_x > tile->x ? data.start_x : tile->x); ++x)
                    data.built[canonical(x, y)] = 1;
            plan_model_remove_built(&data.model, data.built, GRID_SIZE);
        } else
            generate(1);
        data.dragging = 0;
        invalidate();
    }
    return 1;
}
static void draw_piece(const plan_piece* p, int x, int y, int top, color_t color)
{
    float scale = city_view_get_scale() / 100.0f;
    if (p->bridge_sprite >= 0) {
        if (top)
            city_draw_bridge_tile(x, y, scale, p->bridge_sprite, color);
        return;
    }
    if (!p->image)
        return;
    if (top) {
        image_draw_isometric_top_from_draw_tile(p->image, x, y, color, scale);
        if (p->extra_image)
            image_draw(p->extra_image, x + p->extra_x, y + p->extra_y, color, scale);
    } else
        image_draw_isometric_footprint_from_draw_tile(p->image, x, y, color, scale);
}
static void piece(int x, int y, int size, int image, int bridge, int extra, int dx, int dy)
{
    int orientation = city_view_orientation() / 2;
    int ax = x + (orientation >= 2 ? size - 1 : 0);
    int ay = y + (orientation == 0 || orientation == 3 ? size - 1 : 0);
    if (!inside(ax, ay))
        return;
    int offset = map_grid_offset(ax, ay);
    plan_piece p = { .next = -1,
        .image = image,
        .size = size,
        .bridge_sprite = bridge,
        .extra_image = extra,
        .extra_x = dx,
        .extra_y = dy };
    if (preview_drawing) {
        int vx, vy, tx, ty;
        city_view_grid_offset_to_xy_view(offset, &vx, &vy);
        city_view_grid_offset_to_xy_view(preview_offset, &tx, &ty);
        draw_piece(&p, preview_x + (vx - tx) * 60 - 30 * ((vy & 1) - (ty & 1)), preview_y + (vy - ty) * 15,
            preview_top, preview_color);
    } else if (offset >= 0 && offset < CELLS && data.piece_count < PIECES) {
        p.next = data.heads[offset];
        data.heads[offset] = data.piece_count;
        data.pieces[data.piece_count++] = p;
    }
}
static int adjacent_image(const plan_object* p)
{
    int tiles[8] = { 0 };
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 }, dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    const uint8_t* layer = p->type == BUILDING_ROAD ? data.road
        : p->type == BUILDING_WALL                  ? data.wall
                                                    : data.aqueduct;
    int terrain = p->type == BUILDING_ROAD ? TERRAIN_ROAD
        : p->type == BUILDING_WALL         ? TERRAIN_WALL
                                           : TERRAIN_AQUEDUCT;
    for (int d = 0; d < 8; ++d) {
        int x = p->bounds.x + dx[d], y = p->bounds.y + dy[d];
        if (inside(x, y))
            tiles[d] = layer[canonical(x, y)] || map_terrain_is(map_grid_offset(x, y), terrain);
    }
    return map_image_context_planning_offset(terrain, tiles);
}
static void emit_object(const plan_object* p)
{
    int type = p->type, x = p->bounds.x, y = p->bounds.y;
    building b = { 0 };
    b.type = type;
    b.x = x;
    b.y = y;
    b.grid_offset = map_grid_offset(x, y);
    b.size = p->bounds.w;
    b.variant = p->variant;
    b.subtype.orientation = p->rotation;
    b.monument.phase = MONUMENT_FINISHED;
    if (building_type_is_bridge(type)) {
        int dir = p->bridge_direction, relative = (dir - city_view_orientation() + 8) % 8;
        for (int i = 0; i < p->bridge_length; ++i) {
            int xx = x, yy = y;
            if (dir == 0)
                yy += p->bridge_length - 1 - i;
            else if (dir == 2)
                xx += i;
            else if (dir == 4)
                yy += i;
            else
                xx += p->bridge_length - 1 - i;
            piece(xx, yy, 1, 0,
                map_bridge_get_sprite_id(i, p->bridge_length, relative, type == BUILDING_SHIP_BRIDGE), 0, 0,
                0);
        }
        return;
    }
    if (type == BUILDING_ROAD || type == BUILDING_WALL || type == BUILDING_AQUEDUCT) {
        int group = type == BUILDING_ROAD ? GROUP_TERRAIN_ROAD
            : type == BUILDING_WALL       ? GROUP_BUILDING_WALL
                                          : GROUP_BUILDING_AQUEDUCT;
        piece(
            x, y, 1, image_group(group) + adjacent_image(p) + (type == BUILDING_ROAD ? 49 : 0), -1, 0, 0, 0);
        return;
    }
    if (building_is_farm(type)) {
        piece(x, y, 2, image_group(GROUP_BUILDING_FARM_HOUSE), -1, 0, 0, 0);
        for (int yy = 0; yy < 3; ++yy)
            for (int xx = 0; xx < 3; ++xx)
                if (xx == 2 || yy == 2)
                    piece(x + xx, y + yy, 1, building_image_get_base_farm_crop(type), -1, 0, 0, 0);
        return;
    }
    if (type == BUILDING_WAREHOUSE) {
        static const int cx[4] = { 0, 0, 2, 2 }, cy[4] = { 0, 2, 2, 0 };
        for (int yy = 0; yy < 3; ++yy)
            for (int xx = 0; xx < 3; ++xx) {
                int tower = xx == cx[p->rotation] && yy == cy[p->rotation];
                piece(x + xx, y + yy, 1,
                    image_group(tower ? GROUP_BUILDING_WAREHOUSE : GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY),
                    -1, tower ? image_group(GROUP_BUILDING_WAREHOUSE) + 17 : 0, -4, -42);
            }
        return;
    }
    if (type == BUILDING_HIPPODROME) {
        int orientation = building_rotation_get_building_orientation(p->rotation) / 2;
        int group = image_group(orientation % 2 ? GROUP_BUILDING_HIPPODROME_1 : GROUP_BUILDING_HIPPODROME_2);
        for (int i = 0; i < 3; ++i) {
            int xx = x + (p->bounds.w == 15 ? i * 5 : 0), yy = y + (p->bounds.h == 15 ? i * 5 : 0);
            int image_part = (city_view_orientation() == 0 || city_view_orientation() == 6) ? i : 2 - i;
            piece(xx, yy, 5, group + image_part * 2, -1, 0, 0, 0);
        }
        return;
    }
    if (is_fort(type)) {
        piece(x, y, 3, building_image_get(&b), -1, 0, 0, 0);
        piece(p->secondary.x, p->secondary.y, 4, image_group(GROUP_BUILDING_FORT) + 1, -1, 0, 0, 0);
        return;
    }
    if (type == BUILDING_DOCK)
        b.data.dock.orientation = p->rotation;
    if (type == BUILDING_WHARF || type == BUILDING_SHIPYARD)
        b.data.industry.orientation = p->rotation;
    /* Native house image selection consumes the simulation RNG in upstream.
     * Use its fixed property image in this purely visual layer instead. */
    int image = (type == BUILDING_NATIVE_HUT || type == BUILDING_NATIVE_HUT_ALT
                    || type == BUILDING_NATIVE_HUT_ALT_2)
        ? image_group(building_properties_for_type(type)->image_group)
        : building_image_get(&b);
    if (!image) {
        const building_properties* props = building_properties_for_type(type);
        image = image_group(props->image_group) + props->image_offset;
    }
    int extra = 0, dx = 0, dy = 0;
    if (type == BUILDING_GRANARY) {
        extra = image + 1;
        dx = -32 + 30 * (p->bounds.w - 1);
        dy = -64 - 15 * (p->bounds.w - 1);
    }
    if (type == BUILDING_TRIUMPHAL_ARCH) {
        extra = image + 1;
        int rotated = (image != image_group(GROUP_BUILDING_TRIUMPHAL_ARCH));
        dx = (rotated ? -33 : 4) + 30 * (p->bounds.w - 1);
        dy = (rotated ? -56 : -51) - 15 * (p->bounds.w - 1);
    }
    piece(x, y, p->bounds.w, image, -1, extra, dx, dy);
}
void planning_prepare_draw(void)
{
    if (!data.visible)
        return;
    if (!data.dirty && data.orientation == city_view_orientation())
        return;
    memset(data.heads, -1, sizeof(data.heads));
    data.piece_count = 0;
    memset(data.road, 0, sizeof(data.road));
    memset(data.wall, 0, sizeof(data.wall));
    memset(data.aqueduct, 0, sizeof(data.aqueduct));
    for (unsigned i = 0; i < data.model.count; ++i) {
        const plan_object* p = data.model.objects + i;
        uint8_t* layer = p->type == BUILDING_ROAD ? data.road
            : p->type == BUILDING_WALL            ? data.wall
            : p->type == BUILDING_AQUEDUCT        ? data.aqueduct
                                                  : NULL;
        if (layer)
            layer[canonical(p->bounds.x, p->bounds.y)] = 1;
    }
    for (unsigned i = 0; i < data.model.count; ++i)
        emit_object(data.model.objects + i);
    data.dirty = 0;
    data.orientation = city_view_orientation();
}
static void draw_layer(int x, int y, int offset, int top)
{
    if (!data.visible || offset < 0 || offset >= CELLS)
        return;
    for (int i = data.heads[offset]; i >= 0; i = data.pieces[i].next)
        draw_piece(data.pieces + i, x, y, top, GHOST_COLOR);
}
void planning_draw_footprint(int x, int y, int offset) { draw_layer(x, y, offset, 0); }
void planning_draw_top(int x, int y, int offset) { draw_layer(x, y, offset, 1); }
void planning_draw_preview(const map_tile* tile)
{
    if (!data.enabled || !tile->grid_offset || !inside(tile->x, tile->y))
        return;
    preview_drawing = 1;
    preview_offset = tile->grid_offset;
    city_view_get_selected_tile_pixels(&preview_x, &preview_y);
    /* A tile's selected position is its top corner. Cache pieces use the left
     * draw tile, so the conversion below uses lookup coordinates for both. */
    data.end_x = tile->x;
    data.end_y = tile->y;
    preview_color = PREVIEW_COLOR;
    for (preview_top = 0; preview_top < 2; ++preview_top) {
        if (data.tool == 1 && data.selected >= 0) {
            plan_object p = data.model.objects[data.selected];
            int dx = tile->x - data.grabbed_x, dy = tile->y - data.grabbed_y;
            p.bounds.x += dx;
            p.bounds.y += dy;
            if (p.secondary.w) {
                p.secondary.x += dx;
                p.secondary.y += dy;
            }
            if (plan_object_valid(&p, map_data.width, map_data.height, BUILDING_TYPE_MAX))
                emit_object(&p);
        } else if (data.tool == 2 || building_construction_type() == BUILDING_CLEAR_LAND) {
            int sx = data.dragging ? data.start_x : tile->x, sy = data.dragging ? data.start_y : tile->y;
            for (int y = sy < tile->y ? sy : tile->y; y <= (sy > tile->y ? sy : tile->y); ++y)
                for (int x = sx < tile->x ? sx : tile->x; x <= (sx > tile->x ? sx : tile->x); ++x)
                    if (preview_top) {
                        int vx, vy, tx, ty;
                        city_view_grid_offset_to_xy_view(map_grid_offset(x, y), &vx, &vy);
                        city_view_grid_offset_to_xy_view(preview_offset, &tx, &ty);
                        image_blend_footprint_color(preview_x + (vx - tx) * 60 - 30 * ((vy & 1) - (ty & 1)),
                            preview_y + (vy - ty) * 15, INVALID_COLOR, city_view_get_scale() / 100.0f);
                    }
        } else if (!data.tool)
            generate(0);
    }
    preview_drawing = 0;
}
void planning_before_construction(void)
{
    data.snapshot_valid = data.model.count > 0;
    if (!data.snapshot_valid)
        return;
    for (int y = 0; y < map_data.height; ++y)
        for (int x = 0; x < map_data.width; ++x) {
            int c = canonical(x, y), g = map_grid_offset(x, y);
            data.before_terrain[c] = map_terrain_get(g);
            data.before_building[c] = map_building_at(g);
        }
}
void planning_after_construction(void)
{
    if (!data.snapshot_valid)
        return;
    data.snapshot_valid = 0;
    memset(data.built, 0, sizeof(data.built));
    const unsigned structures = TERRAIN_BUILDING | TERRAIN_ROAD | TERRAIN_WALL | TERRAIN_AQUEDUCT
        | TERRAIN_GARDEN | TERRAIN_HIGHWAY | TERRAIN_GATEHOUSE;
    for (int y = 0; y < map_data.height; ++y)
        for (int x = 0; x < map_data.width; ++x) {
            int c = canonical(x, y), g = map_grid_offset(x, y);
            unsigned terrain = map_terrain_get(g), building = map_building_at(g);
            data.built[c] = (building && building != data.before_building[c])
                || ((terrain & structures) & ~data.before_terrain[c]);
        }
    plan_model_remove_built(&data.model, data.built, GRID_SIZE);
    data.undo_valid = 0;
    invalidate();
}
void planning_load(const char* filename)
{
    planning_reset();
    int result = plan_storage_load(filename, &data.model, map_data.width, map_data.height, BUILDING_TYPE_MAX);
    if (result == 1) {
        for (unsigned i = 0; i < data.model.count; ++i)
            if (!supported(data.model.objects[i].type)) {
                data.model.count = 0;
                result = -1;
                break;
            }
    }
    if (result < 0) {
        data.status = "City loaded. Its plan file was invalid or belonged to another save.";
        log_error("Planning: ignored missing, corrupt or mismatched companion", filename, 0);
    }
    invalidate();
}
void planning_save(const char* filename)
{
    if (!plan_storage_save(filename, &data.model, map_data.width, map_data.height, BUILDING_TYPE_MAX)) {
        data.status = "City saved, but its plan file could not be written.";
        log_error("Planning: companion write failed; game save is unaffected", filename, 0);
    } else
        data.status = 0;
}
void planning_copy(const char* source, const char* destination)
{
    if (!plan_storage_copy(source, destination))
        log_info("Planning: no companion copied", source, 0);
}

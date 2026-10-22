#ifndef PLANNING_MODEL_H
#define PLANNING_MODEL_H
#include <stdint.h>
#define PLAN_CAPACITY (162 * 162)
#define PLAN_MAX_DIMENSION 162
/* Canonical map coordinates, never view coordinates or game building ids. */
typedef struct {
    int32_t x, y, w, h;
} plan_rect;
typedef struct {
    uint32_t type, rotation, variant;
    plan_rect bounds, secondary;
    int32_t bridge_direction, bridge_length;
} plan_object;
typedef struct {
    uint32_t count;
    plan_object objects[PLAN_CAPACITY];
} plan_model;
int plan_object_valid(const plan_object* p, int width, int height, unsigned max_type);
int plan_object_contains(const plan_object* p, int x, int y);
int plan_model_add(plan_model* m, const plan_object* p, int width, int height, unsigned max_type);
int plan_model_find(const plan_model* m, int x, int y);
void plan_model_remove(plan_model* m, unsigned index);
int plan_model_move(plan_model* m, unsigned index, int dx, int dy, int width, int height, unsigned max_type);
/* One bit per canonical map tile. Every object hit is removed in its entirety.
 * Roads/walls/aqueducts are individual objects per segment. */
void plan_model_remove_built(plan_model* m, const uint8_t* tiles, int stride);
#endif

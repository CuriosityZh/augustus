#include "model.h"
#include <string.h>
static int rect_valid(plan_rect r, int w, int h)
{
    return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.w <= w && r.h <= h && r.x <= w - r.w
        && r.y <= h - r.h;
}
int plan_object_valid(const plan_object* p, int w, int h, unsigned max_type)
{
    if (!p || w < 1 || h < 1 || w > PLAN_MAX_DIMENSION || h > PLAN_MAX_DIMENSION || !p->type
        || p->type > max_type || p->rotation > 3 || p->variant > 255 || !rect_valid(p->bounds, w, h)
        || p->bridge_length < 0 || p->bridge_length > PLAN_MAX_DIMENSION || p->bridge_direction < 0
        || p->bridge_direction > 6 || p->bridge_direction % 2)
        return 0;
    if (p->secondary.w || p->secondary.h)
        return rect_valid(p->secondary, w, h);
    return p->secondary.x == 0 && p->secondary.y == 0;
}
static int contains(plan_rect r, int x, int y)
{
    return x >= r.x && y >= r.y && x - r.x < r.w && y - r.y < r.h;
}
int plan_object_contains(const plan_object* p, int x, int y)
{
    return contains(p->bounds, x, y) || contains(p->secondary, x, y);
}
int plan_model_add(plan_model* m, const plan_object* p, int w, int h, unsigned max_type)
{
    if (m->count >= PLAN_CAPACITY || !plan_object_valid(p, w, h, max_type))
        return 0;
    /* Repeated painting is idempotent, including when revisiting a route. */
    for (unsigned i = 0; i < m->count; ++i) {
        if (!memcmp(&m->objects[i], p, sizeof(*p)))
            return 1;
    }
    m->objects[m->count++] = *p;
    return 1;
}
int plan_model_find(const plan_model* m, int x, int y)
{
    for (int i = (int)m->count - 1; i >= 0; --i)
        if (plan_object_contains(&m->objects[i], x, y))
            return i;
    return -1;
}
void plan_model_remove(plan_model* m, unsigned i)
{
    if (i >= m->count)
        return;
    memmove(m->objects + i, m->objects + i + 1, (--m->count - i) * sizeof(plan_object));
}
int plan_model_move(plan_model* m, unsigned i, int dx, int dy, int w, int h, unsigned max_type)
{
    if (i >= m->count || dx < -PLAN_MAX_DIMENSION || dx > PLAN_MAX_DIMENSION || dy < -PLAN_MAX_DIMENSION
        || dy > PLAN_MAX_DIMENSION)
        return 0;
    plan_object p = m->objects[i];
    p.bounds.x += dx;
    p.bounds.y += dy;
    if (p.secondary.w) {
        p.secondary.x += dx;
        p.secondary.y += dy;
    }
    if (!plan_object_valid(&p, w, h, max_type))
        return 0;
    m->objects[i] = p;
    return 1;
}
static int rect_hit(plan_rect r, const uint8_t* tiles, int stride)
{
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            if (tiles[y * stride + x])
                return 1;
    return 0;
}
void plan_model_remove_built(plan_model* m, const uint8_t* tiles, int stride)
{
    unsigned out = 0;
    for (unsigned i = 0; i < m->count; ++i) {
        plan_object* p = m->objects + i;
        if (!rect_hit(p->bounds, tiles, stride) && !rect_hit(p->secondary, tiles, stride))
            m->objects[out++] = *p;
    }
    m->count = out;
}

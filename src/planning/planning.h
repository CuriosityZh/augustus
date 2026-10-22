#ifndef PLANNING_PLANNING_H
#define PLANNING_PLANNING_H
#include "input/keys.h"
#include "input/mouse.h"
#include "map/grid.h"
#include "map/point.h"
void planning_reset(void);
int planning_enabled(void);
int planning_key(key_type key, key_modifier_type modifiers, int repeat);
int planning_mouse(const mouse* m, const map_tile* tile);
void planning_draw_controls(void);
void planning_prepare_draw(void);
void planning_draw_footprint(int x, int y, int offset);
void planning_draw_top(int x, int y, int offset);
void planning_draw_preview(const map_tile* tile);
void planning_undo(void);
void planning_before_construction(void);
void planning_after_construction(void);
void planning_load(const char* filename);
void planning_save(const char* filename);
void planning_copy(const char* source, const char* destination);
#endif

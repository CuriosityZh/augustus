#ifndef PLANNING_STORAGE_H
#define PLANNING_STORAGE_H
#include "model.h"
/* Saving reads the game file, but NEVER opens it for writing. Only .planning
 * and a temporary companion in that same directory are written. */
int plan_storage_save(const char* save, const plan_model* m, int width, int height, unsigned max_type);
/* 1: loaded; 0: absent; -1: corrupt, mismatched, unsupported, or I/O failure.
 * Every unsuccessful load leaves m empty. */
int plan_storage_load(const char* save, plan_model* m, int width, int height, unsigned max_type);
int plan_storage_copy(const char* source_save, const char* dest_save);
#endif

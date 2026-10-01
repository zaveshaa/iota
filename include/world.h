#ifndef WORLD_H
#define WORLD_H

#include "vec3.h"

#define WORLD_MAX 512

typedef enum { OBJ_BOX, OBJ_SPHERE } ObjType;

typedef struct {
    Vec3 pos;
    Vec3 half;  // half extents, radius in x for a sphere
    ObjType type;
} Obj;

typedef struct {
    Obj items[WORLD_MAX];
    int count;
} World;

typedef struct {
    float t;
    int obj;
    Vec3 normal;
} Hit;

void world_init(World *w);
int  world_add(World *w, Obj o);
void world_remove(World *w, int idx);
int  world_cast(const World *w, Vec3 origin, Vec3 dir, float max_t, Hit *hit);

int  world_save(const World *w, const char *path);
int  world_load(World *w, const char *path);

#endif

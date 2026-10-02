#ifndef WORLD_H
#define WORLD_H

#include "mesh.h"
#include "vec3.h"

#define WORLD_MAX 512

typedef enum { OBJ_BOX, OBJ_SPHERE, OBJ_MESH } ObjType;

typedef struct {
    Vec3 pos;
    Vec3 half;  // half extents, radius in x for a sphere
    int mesh;   // index into Meshes, -1 for primitives
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
int  world_cast(const World *w, const Meshes *ms, Vec3 origin, Vec3 dir, float max_t, Hit *hit);
int  world_bounds(const World *w, const Meshes *ms, const Obj *o, Vec3 *lo, Vec3 *hi);

int  world_save(const World *w, const Meshes *ms, const char *path);
int  world_load(World *w, Meshes *ms, const char *path);

#endif

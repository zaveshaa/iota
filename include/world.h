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
    float yaw;    // radians about Y, 0 is as authored
    float scale;  // 1 is the size it was authored at
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

// the object's own box in the frame it was authored in, before any yaw or
// scale. every hit test and every collision runs in that frame.
int  world_local_box(const World *w, const Meshes *ms, const Obj *o, Vec3 *lo, Vec3 *hi);
Vec3 world_to_local(const Obj *o, Vec3 p);
Vec3 world_from_local(const Obj *o, Vec3 p);

int  world_save(const World *w, const Meshes *ms, const char *path);
int  world_load(World *w, Meshes *ms, const char *path);

#endif

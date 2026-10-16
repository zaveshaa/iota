#ifndef WORLD_H
#define WORLD_H

#include "light.h"
#include "mesh.h"
#include "vec3.h"

#define RGB(r, g, b)                                                       \
    ((((unsigned)(r) & 0xffu) << 16) | (((unsigned)(g) & 0xffu) << 8) |   \
     ((unsigned)(b) & 0xffu))

typedef enum {
    OBJ_BOX,
    OBJ_SPHERE,
    OBJ_PLANE,
    OBJ_MESH
} ObjKind;

enum {
    OBJ_NONE = 0,
    OBJ_MIRROR = 1u << 0,
    OBJ_TILE = 1u << 1
};

typedef struct {
    ObjKind kind;
    Vec3 pos;
    Vec3 half;
    Vec3 axis;
    unsigned ink;
    unsigned flags;
    int mesh;
} Obj;

#define SCENE_MAX_OBJS 64
#define SCENE_MAX_LIGHTS 8

// where a pool of water sits: a rectangle in the ground plane and the height
// its surface rests at, with the spacing the ripple grid is laid out on
typedef struct {
    Vec3 lo;
    Vec3 hi;
    float level;
    float spacing;
} WaterSpec;

typedef struct {
    Obj objs[SCENE_MAX_OBJS];
    int obj_count;
    Light lights[SCENE_MAX_LIGHTS];
    int light_count;
    unsigned ambient;
    unsigned fog;
    float fog_start;
    float fog_range;
    unsigned sky_top;
    unsigned sky_horizon;
    Vec3 spawn;
    int have_spawn;
    WaterSpec water;
    int have_water;
} Scene;

void scene_clear(Scene *s);
Obj *scene_add_obj(Scene *s);
Light *scene_add_light(Scene *s);

#endif

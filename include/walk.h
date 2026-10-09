#ifndef WALK_H
#define WALK_H

#include "vec3.h"
#include "world.h"

typedef struct {
    Vec3 pos;
    Vec3 vel;
    float radius;
    float height;
    float step;
    int on_ground;
    int jump;
} Walk;

void walk_move(const Scene *s, const Meshes *ms, Walk *w, Vec3 wish,
               float speed, float dt);

#endif

#ifndef CAST_H
#define CAST_H

#include "world.h"

typedef enum {
    HIT_NONE,
    HIT_CONTINUE,
    HIT_MIRROR
} HitAction;

typedef struct {
    HitAction action;
    float t;
    Vec3 point;
    Vec3 normal;
    const Obj *obj;
} Hit;

Hit cast_ray(const Scene *s, const Meshes *ms, Vec3 origin, Vec3 dir,
             float tmax);

#endif

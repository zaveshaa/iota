#ifndef LIGHT_H
#define LIGHT_H

#include "vec3.h"

typedef enum {
    LIGHT_SUN,
    LIGHT_POINT
} LightKind;

typedef struct {
    LightKind kind;
    Vec3 pos;
    Vec3 dir;
    Vec3 color;
    float intensity;
} Light;

#endif

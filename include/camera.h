#ifndef CAMERA_H
#define CAMERA_H

#include "vec3.h"

typedef struct {
    Vec3 pos;
    float yaw;
    float pitch;
} Camera;

void cam_basis(Camera cam, Vec3 *forward, Vec3 *right, Vec3 *up);

#endif

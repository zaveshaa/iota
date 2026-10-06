#include "camera.h"

void cam_basis(Camera cam, Vec3 *forward, Vec3 *right, Vec3 *up) {
    float cy = __builtin_cosf(cam.yaw), sy = __builtin_sinf(cam.yaw);
    float cp = __builtin_cosf(cam.pitch), sp = __builtin_sinf(cam.pitch);

    *forward = v3(sy * cp, sp, cy * cp);
    *right   = v3(cy, 0.0f, -sy);
    *up      = v3_cross(*forward, *right);
}

#ifndef VIEW_H
#define VIEW_H

#include "cast.h"
#include "render.h"
#include "world.h"

#define VIEW_MAX_DEPTH 2
#define VIEW_BUDGET_MS 11.0

typedef struct {
    Vec3 pos;
    float yaw;
    float pitch;
    float fov;
} Camera;

typedef struct {
    unsigned rays;
    int depth_max;
    int budget_hit;
} ViewStats;

void view_render(const Scene *s, const Camera *cam, Render *r, int ss,
                 float budget_ms, ViewStats *st);

#endif

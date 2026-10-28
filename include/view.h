#ifndef VIEW_H
#define VIEW_H

#include "cast.h"
#include "render.h"
#include "water.h"
#include "world.h"

#define VIEW_MAX_DEPTH 2
#define VIEW_BUDGET_MS 15.0
#define VIEW_RAMP " .:-=+*#%@"

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

// how many samples each axis of a cell gathers before it settles on a glyph and
// a colour; past four the cost climbs faster than the eye can tell
#define VIEW_SS_MAX 4

void view_render(const Scene *s, const Meshes *ms, const Water *w,
                 const Camera *cam, Render *r, int ss, float budget_ms,
                 ViewStats *st);

#endif

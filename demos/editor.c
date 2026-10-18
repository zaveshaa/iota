#include "engine.h"
#include "input.h"
#include "level.h"
#include "render.h"
#include "term.h"
#include "ui.h"
#include "view.h"
#include "water.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLY_SPEED 9.0f
#define LOOK_SPEED 2.0f
#define PITCH_MAX 1.5f
#define REACH 200.0f
#define MIN_HALF 0.05f
#define TAU 6.28318530718f
#define QUARTER 1.57079632679f
#define EIGHTH 0.78539816339f
#define GRID_SPAN 20
#define PANEL_W 30

typedef enum {
    TOOL_BOX,
    TOOL_WALL,
    TOOL_WATER,
    TOOL_STAIRS,
    TOOL_TUNNEL,
    TOOL_PIT,
    TOOL_SPAWN,
    TOOL_LIGHT,
    TOOL_SELECT,
    TOOL_PAINT,
    TOOL_ERASE,
    TOOL_COUNT
} Tool;

// the first ten tools answer to the number row, 1 .. 9 then 0; the rest are a
// tab or a panel click away
#define TOOL_KEYS 10

static const char *const TOOL_NAME[TOOL_COUNT] = {
    "box",   "wall",  "water", "stairs", "tunnel", "pit",
    "spawn", "light", "select", "paint", "erase"
};

static const char *const TOOL_HINT[TOOL_COUNT] = {
    "two corners make a solid",
    "the near end, then the far end",
    "corner a pool, then the far corner",
    "the foot, then the top",
    "corner a bore, then the far corner",
    "corner a hole, then the far corner",
    "where the body starts",
    "where a lamp hangs",
    "click a panel or a lamp to pick it",
    "turn a block's material over",
    "take the panel under the cursor"
};

typedef struct {
    const char *name;
    unsigned ink;
    unsigned flags;
} Material;

static const Material MATS[] = {
    {"wall", RGB(150, 150, 152), 0u},
    {"tile", RGB(96, 100, 116), OBJ_TILE},
    {"mirror", RGB(84, 92, 112), OBJ_MIRROR},
    {"shoji", RGB(224, 214, 192), OBJ_SHOJI},
    {"glass", RGB(176, 208, 220), OBJ_GLASS}
};

#define MAT_COUNT ((int)(sizeof MATS / sizeof MATS[0]))

#define HIST_MAX 128

typedef struct {
    Scene scene;
    Scene scratch;
    Scene *hist;
    int hist_cap;
    int hist_len;
    int hist_pos;
    Meshes meshes;
    Water water;
    Camera cam;
    Input input;
    Render *render;
    Ui ui;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    float tan_half;
    float aspect;
    int cols;
    int rows;
    int mx;
    int my;
    int mat;
    Tool tool;
    int snap;
    int grid;
    int help;
    int anchor_set;
    Vec3 anchor;
    float last_t;
    int sel;
    int sel_light;
    float riser;
    float stair_w;
    float wall;
    float wall_h;
    float depth;
    float lamp_i;
    float res;
    int click;
    int rclick;
    int btn_down;
    char path[256];
    char msg[80];
} App;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int lower(int k)
{
    return (k >= 'A' && k <= 'Z') ? k + 32 : k;
}

static void say(App *a, const char *text)
{
    (void)snprintf(a->msg, sizeof a->msg, "%s", text);
}

static float snapf(float v)
{
    return roundf(v * 2.0f) * 0.5f;
}

static Vec3 snapv(const App *a, Vec3 p)
{
    if (!a->snap) {
        return p;
    }
    return v3(snapf(p.x), snapf(p.y), snapf(p.z));
}

static void refresh_basis(App *a)
{
    float cp = cosf(a->cam.pitch);

    a->forward = v3(cp * sinf(a->cam.yaw), sinf(a->cam.pitch),
                    cp * cosf(a->cam.yaw));
    a->right = v3_norm(v3_cross(a->forward, v3(0.0f, 1.0f, 0.0f)));
    a->up = v3_cross(a->right, a->forward);
    a->tan_half = tanf(a->cam.fov * 0.5f);
    a->cols = render_cols(a->render);
    a->rows = render_rows(a->render);
    a->aspect = (float)a->cols / (float)a->rows;
}

static Vec3 mouse_dir(const App *a)
{
    float px = (2.0f * ((float)a->mx + 0.5f) / (float)a->cols - 1.0f) *
               a->aspect * a->tan_half;
    float py = (1.0f - 2.0f * ((float)a->my + 0.5f) / (float)a->rows) *
               a->tan_half;

    return v3_norm(v3_add(a->forward, v3_add(v3_mul(a->right, px),
                                            v3_mul(a->up, py))));
}

static int to_cell(const App *a, Vec3 p, int *cx, int *cy)
{
    Vec3 rel = v3_sub(p, a->cam.pos);
    float z = v3_dot(rel, a->forward);
    float xr;
    float yu;

    if (z <= 0.05f) {
        return 0;
    }
    xr = v3_dot(rel, a->right) / (z * a->aspect * a->tan_half);
    yu = v3_dot(rel, a->up) / (z * a->tan_half);
    *cx = (int)((xr + 1.0f) * 0.5f * (float)a->cols);
    *cy = (int)((1.0f - yu) * 0.5f * (float)a->rows);
    return *cx >= 0 && *cx < a->cols && *cy >= 0 && *cy < a->rows;
}

static void plot(App *a, Vec3 p, unsigned char ch, unsigned ink)
{
    int cx;
    int cy;

    if (to_cell(a, p, &cx, &cy)) {
        render_put(a->render, cx, cy, ch, ink);
    }
}

static void line3(App *a, Vec3 p, Vec3 q, unsigned char ch, unsigned ink)
{
    int steps = 24;
    int i;

    for (i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;

        plot(a, v3_add(p, v3_mul(v3_sub(q, p), t)), ch, ink);
    }
}

// one of the two points a span tool watches; the ray lands on whatever the
// cursor saw, and past the last surface the point sits out along the ray so a
// corner can still be hung in the open air
static Vec3 cursor_point(App *a, const Obj **obj, Vec3 *normal)
{
    Vec3 dir = mouse_dir(a);
    Hit hit = cast_ray(&a->scene, &a->meshes, a->cam.pos, dir, REACH);
    Vec3 p;

    if (hit.action != HIT_NONE) {
        a->last_t = hit.t;
        if (obj != NULL) {
            *obj = hit.obj;
        }
        if (normal != NULL) {
            *normal = hit.normal;
        }
        return snapv(a, hit.point);
    }
    if (obj != NULL) {
        *obj = NULL;
    }
    if (normal != NULL) {
        *normal = v3(0.0f, 1.0f, 0.0f);
    }
    p = v3_add(a->cam.pos, v3_mul(dir, a->last_t));
    return snapv(a, p);
}

static int sees(App *a, Vec3 p)
{
    Vec3 rel = v3_sub(p, a->cam.pos);
    float d = v3_len(rel);
    Hit h;

    if (d < 1e-3f) {
        return 1;
    }
    h = cast_ray(&a->scene, &a->meshes, a->cam.pos, v3_mul(rel, 1.0f / d),
                 d - 0.02f);
    return h.action == HIT_NONE;
}

static void gpoint(App *a, Vec3 p, unsigned ink)
{
    if (sees(a, p)) {
        plot(a, p, (unsigned char)'.', ink);
    }
}

static void draw_grid(App *a)
{
    int i;
    int j;

    for (i = -GRID_SPAN; i <= GRID_SPAN; i++) {
        unsigned ink = (i % 5 == 0) ? RGB(74, 92, 120) : RGB(46, 56, 74);

        for (j = -GRID_SPAN; j <= GRID_SPAN; j++) {
            gpoint(a, v3((float)j, 0.0f, (float)i), ink);
            gpoint(a, v3((float)i, 0.0f, (float)j), ink);
        }
    }
    for (i = -GRID_SPAN; i <= GRID_SPAN; i++) {
        gpoint(a, v3((float)i, 0.0f, 0.0f), RGB(178, 84, 96));
        gpoint(a, v3(0.0f, 0.0f, (float)i), RGB(92, 116, 198));
    }
    for (i = 1; i <= GRID_SPAN; i += 2) {
        gpoint(a, v3(0.0f, (float)i, 0.0f), RGB(104, 176, 116));
    }
}

static void box_corners(Vec3 a, Vec3 b, Vec3 *c, Vec3 *h)
{
    *c = v3((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
    *h = v3(fabsf(a.x - b.x) * 0.5f, fabsf(a.y - b.y) * 0.5f,
            fabsf(a.z - b.z) * 0.5f);
}

static void draw_box_edges(App *a, Vec3 c, Vec3 h, Vec3 pad, float yaw,
                           unsigned ink)
{
    Vec3 p[8];
    float cs = cosf(yaw);
    float sn = sinf(yaw);
    int sx;
    int sy;
    int sz;
    int i = 0;
    static const int e[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
        {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (sz = -1; sz <= 1; sz += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            for (sx = -1; sx <= 1; sx += 2) {
                float ox = (h.x + pad.x) * (float)sx;
                float oz = (h.z + pad.z) * (float)sz;

                p[i] = v3(c.x + ox * cs + oz * sn,
                          c.y + (h.y + pad.y) * (float)sy,
                          c.z - ox * sn + oz * cs);
                i++;
            }
        }
    }
    for (i = 0; i < 12; i++) {
        line3(a, p[e[i][0]], p[e[i][1]], (unsigned char)'+', ink);
    }
}

static void water_apply(App *a)
{
    if (a->scene.have_water) {
        water_open(&a->water, a->scene.water.lo, a->scene.water.hi,
                   a->scene.water.level, a->scene.water.spacing);
    } else {
        water_close(&a->water);
    }
}

// The whole scene is a few kilobytes, so history keeps plain copies of it: a
// frame that changed anything banks the new scene and a later undo swaps an old
// one back. Each frame starts with a scratch copy and only banks when the two
// differ, so a click that builds nothing leaves no dead step behind.
static void hist_init(App *a)
{
    a->hist = malloc(sizeof(Scene) * HIST_MAX);
    a->hist_cap = a->hist ? HIST_MAX : 0;
    a->hist_len = 0;
    a->hist_pos = -1;
    if (a->hist != NULL) {
        a->hist[0] = a->scene;
        a->hist_len = 1;
        a->hist_pos = 0;
    }
}

static void hist_commit(App *a)
{
    if (a->hist == NULL) {
        return;
    }
    a->hist_len = a->hist_pos + 1;
    if (a->hist_len >= a->hist_cap) {
        memmove(a->hist, a->hist + 1, sizeof(Scene) * (size_t)(a->hist_cap - 1));
        a->hist_len = a->hist_cap - 1;
        a->hist_pos = a->hist_len - 1;
    }
    a->hist[a->hist_len] = a->scene;
    a->hist_len++;
    a->hist_pos = a->hist_len - 1;
}

static void hist_settle(App *a)
{
    if (a->hist != NULL && memcmp(&a->scratch, &a->scene, sizeof(Scene)) != 0) {
        hist_commit(a);
    }
}

static void hist_show(App *a, int pos, const char *word)
{
    a->scene = a->hist[pos];
    a->scratch = a->scene;
    a->hist_pos = pos;
    a->sel = -1;
    a->sel_light = -1;
    ui_edit_stop(&a->ui);
    water_apply(a);
    say(a, word);
}

static void hist_undo(App *a)
{
    if (a->hist != NULL && a->hist_pos > 0) {
        hist_show(a, a->hist_pos - 1, "undone");
    } else {
        say(a, "nothing to undo");
    }
}

static void hist_redo(App *a)
{
    if (a->hist != NULL && a->hist_pos + 1 < a->hist_len) {
        hist_show(a, a->hist_pos + 1, "redone");
    } else {
        say(a, "nothing to redo");
    }
}

static Obj *add_box(App *a, Vec3 c, Vec3 h)
{
    Obj *o = scene_add_obj(&a->scene);

    if (o == NULL) {
        return NULL;
    }
    o->kind = OBJ_BOX;
    o->pos = c;
    o->half = v3(clampf(h.x, MIN_HALF, 64.0f), clampf(h.y, MIN_HALF, 64.0f),
                 clampf(h.z, MIN_HALF, 64.0f));
    o->ink = MATS[a->mat].ink;
    o->flags = MATS[a->mat].flags;
    return o;
}

static void place_box(App *a, Vec3 lo, Vec3 hi)
{
    Vec3 c;
    Vec3 h;

    box_corners(lo, hi, &c, &h);
    if (add_box(a, c, h) == NULL) {
        say(a, "no room left for another panel");
        return;
    }
    (void)snprintf(a->msg, sizeof a->msg, "laid a %s block",
                   MATS[a->mat].name);
}

static void place_wall(App *a, Vec3 lo, Vec3 hi)
{
    float dx = hi.x - lo.x;
    float dz = hi.z - lo.z;
    float len = sqrtf(dx * dx + dz * dz);
    float h = clampf(a->wall_h, MIN_HALF, 64.0f);
    float th = clampf(a->wall, MIN_HALF, 4.0f);
    Obj *o;

    if (len < 0.2f) {
        say(a, "a wall wants a longer run");
        return;
    }
    o = add_box(a, v3((lo.x + hi.x) * 0.5f, lo.y + h * 0.5f,
                      (lo.z + hi.z) * 0.5f),
                v3(len * 0.5f, h * 0.5f, th * 0.5f));
    if (o == NULL) {
        say(a, "no room left for another wall");
        return;
    }
    o->yaw = atan2f(-dz, dx);
    (void)snprintf(a->msg, sizeof a->msg, "raised a wall %.1f long, %.1f high",
                   (double)len, (double)h);
}

static void place_water(App *a, Vec3 lo, Vec3 hi)
{
    float x0 = lo.x < hi.x ? lo.x : hi.x;
    float x1 = lo.x < hi.x ? hi.x : lo.x;
    float z0 = lo.z < hi.z ? lo.z : hi.z;
    float z1 = lo.z < hi.z ? hi.z : lo.z;

    if (x1 - x0 < 0.5f || z1 - z0 < 0.5f) {
        say(a, "a pool wants a wider rectangle");
        return;
    }
    a->scene.water.lo = v3(x0, 0.0f, z0);
    a->scene.water.hi = v3(x1, 0.0f, z1);
    a->scene.water.level = lo.y;
    a->scene.water.spacing = 0.5f;
    a->scene.have_water = 1;
    water_apply(a);
    (void)snprintf(a->msg, sizeof a->msg, "pool at %.1f wide, level %.1f",
                   (double)(x1 - x0), (double)lo.y);
}

static void place_stairs(App *a, Vec3 lo, Vec3 hi)
{
    float dx = hi.x - lo.x;
    float dz = hi.z - lo.z;
    float rise = hi.y - lo.y;
    float run;
    float sh;
    int n;
    int i;

    if (fabsf(dx) + fabsf(dz) < 0.5f || rise < 0.05f) {
        say(a, "stairs want a longer, higher span");
        return;
    }
    n = (int)lroundf(rise / a->riser);
    if (n < 1) {
        n = 1;
    }
    if (n > SCENE_MAX_OBJS - a->scene.obj_count) {
        n = SCENE_MAX_OBJS - a->scene.obj_count;
    }
    if (n < 1) {
        say(a, "no room left for the staircase");
        return;
    }
    run = (fabsf(dx) + fabsf(dz)) / (float)n;
    sh = rise / (float)n;
    for (i = 0; i < n; i++) {
        float top = lo.y + sh * (float)(i + 1);
        Obj *o;

        if (fabsf(dx) >= fabsf(dz)) {
            float dir = dx >= 0.0f ? 1.0f : -1.0f;

            o = add_box(a, v3(lo.x + dir * run * ((float)i + 0.5f),
                              (lo.y + top) * 0.5f, lo.z),
                        v3(run * 0.5f, (top - lo.y) * 0.5f, a->stair_w * 0.5f));
        } else {
            float dir = dz >= 0.0f ? 1.0f : -1.0f;

            o = add_box(a, v3(lo.x, (lo.y + top) * 0.5f,
                              lo.z + dir * run * ((float)i + 0.5f)),
                        v3(a->stair_w * 0.5f, (top - lo.y) * 0.5f, run * 0.5f));
        }
        if (o == NULL) {
            break;
        }
    }
    (void)snprintf(a->msg, sizeof a->msg, "laid %d steps up", n);
}

static void place_tunnel(App *a, Vec3 lo, Vec3 hi)
{
    float x0 = lo.x < hi.x ? lo.x : hi.x;
    float x1 = lo.x < hi.x ? hi.x : lo.x;
    float y0 = lo.y < hi.y ? lo.y : hi.y;
    float y1 = lo.y < hi.y ? hi.y : lo.y;
    float z0 = lo.z < hi.z ? lo.z : hi.z;
    float z1 = lo.z < hi.z ? hi.z : lo.z;
    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;
    float cz = (z0 + z1) * 0.5f;
    float w = clampf(a->wall, MIN_HALF, 2.0f);
    float hx = (x1 - x0) * 0.5f;
    float hy = (y1 - y0) * 0.5f;
    float hz = (z1 - z0) * 0.5f;
    float short_side = hx < hz ? hx : hz;
    int ok;

    if (hy < w || short_side < 2.0f * w) {
        say(a, "a tunnel wants a taller bore than its walls");
        return;
    }
    if (a->scene.obj_count + 4 > SCENE_MAX_OBJS) {
        say(a, "no room left for the tunnel");
        return;
    }
    ok = add_box(a, v3(cx, y0 + w * 0.5f, cz), v3(hx, w * 0.5f, hz)) != NULL;
    ok = ok && add_box(a, v3(cx, y1 - w * 0.5f, cz),
                       v3(hx, w * 0.5f, hz)) != NULL;
    if (hx >= hz) {
        ok = ok && add_box(a, v3(cx, cy, z0 + w * 0.5f),
                           v3(hx, hy, w * 0.5f)) != NULL;
        ok = ok && add_box(a, v3(cx, cy, z1 - w * 0.5f),
                           v3(hx, hy, w * 0.5f)) != NULL;
    } else {
        ok = ok && add_box(a, v3(x0 + w * 0.5f, cy, cz),
                           v3(w * 0.5f, hy, hz)) != NULL;
        ok = ok && add_box(a, v3(x1 - w * 0.5f, cy, cz),
                           v3(w * 0.5f, hy, hz)) != NULL;
    }
    if (!ok) {
        say(a, "ran out of room partway through");
        return;
    }
    (void)snprintf(a->msg, sizeof a->msg, "bored a tunnel %.1f long",
                   (double)(x1 - x0 + z1 - z0));
}

static void place_pit(App *a, Vec3 lo, Vec3 hi)
{
    float x0 = lo.x < hi.x ? lo.x : hi.x;
    float x1 = lo.x < hi.x ? hi.x : lo.x;
    float z0 = lo.z < hi.z ? lo.z : hi.z;
    float z1 = lo.z < hi.z ? hi.z : lo.z;
    float y_top = lo.y;
    float w = clampf(a->wall, MIN_HALF, 2.0f);
    float d = clampf(a->depth, w * 2.0f, 64.0f);
    float y_bot = y_top - d;
    float cx = (x0 + x1) * 0.5f;
    float cz = (z0 + z1) * 0.5f;
    float hx = (x1 - x0) * 0.5f;
    float hz = (z1 - z0) * 0.5f;
    float cy = (y_top + y_bot) * 0.5f;
    float hy = d * 0.5f;
    int ok;

    if (hx < 2.0f * w || hz < 2.0f * w) {
        say(a, "a pit wants a wider mouth than its walls");
        return;
    }
    if (a->scene.obj_count + 5 > SCENE_MAX_OBJS) {
        say(a, "no room left for the pit");
        return;
    }
    ok = add_box(a, v3(cx, y_bot + w * 0.5f, cz), v3(hx, w * 0.5f, hz)) != NULL;
    ok = ok && add_box(a, v3(x0 + w * 0.5f, cy, cz),
                       v3(w * 0.5f, hy, hz)) != NULL;
    ok = ok && add_box(a, v3(x1 - w * 0.5f, cy, cz),
                       v3(w * 0.5f, hy, hz)) != NULL;
    ok = ok && add_box(a, v3(cx, cy, z0 + w * 0.5f),
                       v3(hx, hy, w * 0.5f)) != NULL;
    ok = ok && add_box(a, v3(cx, cy, z1 - w * 0.5f),
                       v3(hx, hy, w * 0.5f)) != NULL;
    if (!ok) {
        say(a, "ran out of room partway through");
        return;
    }
    (void)snprintf(a->msg, sizeof a->msg, "sank a pit %.1f deep", (double)d);
}

static void place_light(App *a, Vec3 p)
{
    Light *l = scene_add_light(&a->scene);

    if (l == NULL) {
        say(a, "no room left for another lamp");
        return;
    }
    l->kind = LIGHT_POINT;
    l->pos = v3(p.x, p.y + 0.5f, p.z);
    l->color = v3(1.0f, 0.96f, 0.9f);
    l->intensity = a->lamp_i;
    a->sel_light = a->scene.light_count - 1;
    a->sel = -1;
    say(a, "hung a lamp");
}

static int mat_of(unsigned flags)
{
    int i;

    for (i = 0; i < MAT_COUNT; i++) {
        if (MATS[i].flags == flags) {
            return i;
        }
    }
    return 0;
}

static void cycle_material(Obj *o)
{
    int i = (mat_of(o->flags) + 1) % MAT_COUNT;

    o->flags = MATS[i].flags;
    o->ink = MATS[i].ink;
}

static void paint_under(App *a, const Obj *obj)
{
    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    cycle_material((Obj *)obj);
    (void)snprintf(a->msg, sizeof a->msg, "now a %s block",
                   MATS[mat_of(obj->flags)].name);
}

static void rotate_under(App *a)
{
    const Obj *obj = NULL;

    (void)cursor_point(a, &obj, NULL);
    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    ((Obj *)obj)->yaw += EIGHTH;
    if (((Obj *)obj)->yaw > TAU) {
        ((Obj *)obj)->yaw -= TAU;
    }
    say(a, "turned an eighth of a turn");
}

static void erase_at(App *a, int idx)
{
    int i;

    if (idx < 0 || idx >= a->scene.obj_count) {
        say(a, "nothing under the cursor");
        return;
    }
    for (i = idx; i + 1 < a->scene.obj_count; i++) {
        a->scene.objs[i] = a->scene.objs[i + 1];
    }
    a->scene.obj_count--;
    if (a->sel == idx) {
        a->sel = -1;
    } else if (a->sel > idx) {
        a->sel--;
    }
    say(a, "took that panel away");
}

static void erase_under(App *a, const Obj *obj)
{
    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    erase_at(a, (int)(obj - a->scene.objs));
}

static void select_under(App *a)
{
    Vec3 dir = mouse_dir(a);
    Hit h = cast_ray(&a->scene, &a->meshes, a->cam.pos, dir, REACH);
    float best_t = h.action != HIT_NONE ? h.t : REACH;
    int best_light = -1;
    int i;

    for (i = 0; i < a->scene.light_count; i++) {
        const Light *l = &a->scene.lights[i];
        Vec3 oc = v3_sub(l->pos, a->cam.pos);
        float tca = v3_dot(oc, dir);
        float d2;

        if (tca <= 0.0f) {
            continue;
        }
        d2 = v3_dot(oc, oc) - tca * tca;
        if (d2 > 0.36f) {
            continue;
        }
        if (tca < best_t) {
            best_t = tca;
            best_light = i;
        }
    }
    if (best_light >= 0) {
        a->sel_light = best_light;
        a->sel = -1;
        say(a, "picked a lamp");
        return;
    }
    if (h.action != HIT_NONE && h.obj != NULL) {
        a->sel = (int)(h.obj - a->scene.objs);
        a->sel_light = -1;
        say(a, "picked a panel");
        return;
    }
    a->sel = -1;
    a->sel_light = -1;
    say(a, "nothing to pick");
}

static void world_click(App *a)
{
    const Obj *obj = NULL;
    Vec3 n = v3(0.0f, 1.0f, 0.0f);
    Vec3 p = cursor_point(a, &obj, &n);

    switch (a->tool) {
    case TOOL_SPAWN:
        a->scene.spawn = v3(p.x, p.y, p.z);
        a->scene.have_spawn = 1;
        say(a, "body starts here");
        return;
    case TOOL_LIGHT:
        place_light(a, p);
        return;
    case TOOL_SELECT:
        select_under(a);
        return;
    case TOOL_PAINT:
        paint_under(a, obj);
        return;
    case TOOL_ERASE:
        erase_under(a, obj);
        return;
    default:
        break;
    }
    if (!a->anchor_set) {
        a->anchor = p;
        a->anchor_set = 1;
        say(a, "now click the far corner");
        return;
    }
    if (a->tool == TOOL_BOX) {
        place_box(a, a->anchor, p);
    } else if (a->tool == TOOL_WALL) {
        place_wall(a, a->anchor, p);
    } else if (a->tool == TOOL_WATER) {
        place_water(a, a->anchor, p);
    } else if (a->tool == TOOL_STAIRS) {
        place_stairs(a, a->anchor, p);
    } else if (a->tool == TOOL_TUNNEL) {
        place_tunnel(a, a->anchor, p);
    } else if (a->tool == TOOL_PIT) {
        place_pit(a, a->anchor, p);
    }
    a->anchor_set = 0;
}

static void app_new(App *a)
{
    Obj *o;
    Light *l;

    scene_clear(&a->scene);
    o = scene_add_obj(&a->scene);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, -0.5f, 0.0f);
    o->half = v3(16.0f, 0.5f, 16.0f);
    o->ink = MATS[1].ink;
    o->flags = MATS[1].flags;
    l = scene_add_light(&a->scene);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 6.0f, 0.0f);
    l->color = v3(1.0f, 0.96f, 0.9f);
    l->intensity = a->lamp_i;
    a->scene.spawn = v3(0.0f, 0.0f, 0.0f);
    a->scene.have_spawn = 1;
    a->sel = -1;
    a->sel_light = -1;
    water_apply(a);
}

static void select_tool(App *a, Tool t)
{
    a->tool = t;
    a->anchor_set = 0;
    ui_edit_stop(&a->ui);
    (void)snprintf(a->msg, sizeof a->msg, "%s: %s", TOOL_NAME[t], TOOL_HINT[t]);
}

static void app_poll(Engine *e)
{
    App *a = engine_user(e);
    int k;

    refresh_basis(a);
    a->scratch = a->scene;
    while ((k = term_read_key()) != KEY_NONE) {
        double now = engine_time(e);
        int lc;

        if (k == KEY_MOUSE) {
            a->mx = term_mouse_x();
            a->my = term_mouse_y();
            if (term_mouse_motion()) {
                continue;
            }
            if (term_mouse_press()) {
                int b = term_mouse_button();

                if (b == 0) {
                    a->click = 1;
                    a->btn_down = 1;
                } else if (b == 2) {
                    a->rclick = 1;
                }
            } else {
                a->btn_down = 0;
            }
            continue;
        }
        if (a->ui.editing != 0 && ui_edit_key(&a->ui, k)) {
            continue;
        }
        lc = lower(k);
        input_arrive(&a->input, lc, now);
        switch (lc) {
        case KEY_ESC:
        case 3:
            if (a->anchor_set) {
                a->anchor_set = 0;
                say(a, "cancelled");
            } else {
                engine_quit(e);
            }
            break;
        case 19:
            if (level_save(&a->scene, a->path) == 0) {
                (void)snprintf(a->msg, sizeof a->msg, "wrote %s", a->path);
            } else {
                say(a, "could not write the level");
            }
            break;
        case 15:
            if (level_load(&a->scene, a->path) == 0) {
                water_apply(a);
                a->sel = -1;
                a->sel_light = -1;
                ui_edit_stop(&a->ui);
                (void)snprintf(a->msg, sizeof a->msg, "read %s", a->path);
            } else {
                say(a, "could not read the level");
            }
            break;
        case 9:
            select_tool(a, (Tool)((a->tool + 1) % TOOL_COUNT));
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
            select_tool(a, (Tool)((lc == '0' ? TOOL_KEYS - 1 : lc - '1') % TOOL_COUNT));
            break;
        case 'r':
            rotate_under(a);
            break;
        case 'm':
            a->mat = (a->mat + 1) % MAT_COUNT;
            (void)snprintf(a->msg, sizeof a->msg, "%s blocks", MATS[a->mat].name);
            break;
        case 'g':
            a->snap = !a->snap;
            say(a, a->snap ? "snap on" : "snap off");
            break;
        case 'p':
            select_tool(a, TOOL_SPAWN);
            world_click(a);
            break;
        case 'u':
        case 26:
            hist_undo(a);
            break;
        case 25:
            hist_redo(a);
            break;
        case 'n':
            app_new(a);
            a->scratch = a->scene;
            hist_commit(a);
            say(a, "a fresh level");
            break;
        case 'h':
            a->help = !a->help;
            break;
        default:
            break;
        }
    }
}

static void app_sim(Engine *e, float dt)
{
    App *a = engine_user(e);
    double now = engine_time(e);
    Vec3 wish = v3(0.0f, 0.0f, 0.0f);

    refresh_basis(a);
    if (input_held(&a->input, 'w', now)) {
        wish = v3_add(wish, a->forward);
    }
    if (input_held(&a->input, 's', now)) {
        wish = v3_sub(wish, a->forward);
    }
    if (input_held(&a->input, 'd', now)) {
        wish = v3_add(wish, a->right);
    }
    if (input_held(&a->input, 'a', now)) {
        wish = v3_sub(wish, a->right);
    }
    if (input_held(&a->input, ' ', now)) {
        wish = v3_add(wish, v3(0.0f, 1.0f, 0.0f));
    }
    if (input_held(&a->input, 'c', now)) {
        wish = v3_sub(wish, v3(0.0f, 1.0f, 0.0f));
    }
    if (v3_len(wish) > 0.0001f) {
        a->cam.pos = v3_add(a->cam.pos, v3_mul(v3_norm(wish), FLY_SPEED * dt));
    }
    if (input_held(&a->input, KEY_LEFT, now)) {
        a->cam.yaw += LOOK_SPEED * dt;
    }
    if (input_held(&a->input, KEY_RIGHT, now)) {
        a->cam.yaw -= LOOK_SPEED * dt;
    }
    if (input_held(&a->input, KEY_UP, now)) {
        a->cam.pitch += LOOK_SPEED * 0.7f * dt;
    }
    if (input_held(&a->input, KEY_DOWN, now)) {
        a->cam.pitch -= LOOK_SPEED * 0.7f * dt;
    }
    a->cam.pitch = clampf(a->cam.pitch, -PITCH_MAX, PITCH_MAX);
    water_step(&a->water, dt);
}

static void draw_ghost(App *a)
{
    const Obj *obj = NULL;
    Vec3 p = cursor_point(a, &obj, NULL);

    if (a->sel >= 0 && a->sel < a->scene.obj_count) {
        const Obj *s = &a->scene.objs[a->sel];

        draw_box_edges(a, s->pos, s->half, v3(0.05f, 0.05f, 0.05f), s->yaw,
                       RGB(120, 240, 220));
    }
    if (a->sel_light >= 0 && a->sel_light < a->scene.light_count) {
        Vec3 lp = a->scene.lights[a->sel_light].pos;

        line3(a, v3(lp.x - 0.5f, lp.y, lp.z), v3(lp.x + 0.5f, lp.y, lp.z),
              (unsigned char)'-', RGB(255, 230, 150));
        line3(a, v3(lp.x, lp.y - 0.5f, lp.z), v3(lp.x, lp.y + 0.5f, lp.z),
              (unsigned char)'|', RGB(255, 230, 150));
        line3(a, v3(lp.x, lp.y, lp.z - 0.5f), v3(lp.x, lp.y, lp.z + 0.5f),
              (unsigned char)'-', RGB(255, 230, 150));
    }

    if (a->tool == TOOL_ERASE) {
        if (obj != NULL) {
            draw_box_edges(a, obj->pos, obj->half, v3(0.04f, 0.04f, 0.04f),
                           obj->yaw, RGB(255, 110, 110));
        }
        return;
    }
    if (a->tool == TOOL_SELECT || a->tool == TOOL_PAINT ||
        a->tool == TOOL_LIGHT) {
        draw_box_edges(a, p, v3(0.12f, 0.12f, 0.12f), v3(0.0f, 0.0f, 0.0f),
                       0.0f, RGB(255, 236, 140));
        return;
    }
    if (a->tool == TOOL_SPAWN) {
        draw_box_edges(a, v3(p.x, p.y + 0.2f, p.z), v3(0.3f, 0.2f, 0.3f),
                       v3(0.0f, 0.0f, 0.0f), 0.0f, RGB(120, 240, 140));
        line3(a, p, v3(p.x, p.y + 1.4f, p.z), (unsigned char)'|',
              RGB(120, 240, 140));
        return;
    }

    if (!a->anchor_set) {
        draw_box_edges(a, p, v3(0.12f, 0.12f, 0.12f), v3(0.0f, 0.0f, 0.0f),
                       0.0f, RGB(255, 236, 140));
        return;
    }

    draw_box_edges(a, a->anchor, v3(0.1f, 0.1f, 0.1f), v3(0.0f, 0.0f, 0.0f),
                   0.0f, RGB(120, 220, 255));

    if (a->tool == TOOL_BOX || a->tool == TOOL_TUNNEL) {
        Vec3 c;
        Vec3 h;

        box_corners(a->anchor, p, &c, &h);
        draw_box_edges(a, c, h, v3(0.0f, 0.0f, 0.0f), 0.0f, RGB(255, 236, 140));
    } else if (a->tool == TOOL_PIT) {
        Vec3 c;
        Vec3 h;
        Vec3 floor = v3(p.x, a->anchor.y - a->depth, p.z);

        box_corners(a->anchor, floor, &c, &h);
        draw_box_edges(a, c, h, v3(0.0f, 0.0f, 0.0f), 0.0f, RGB(255, 170, 120));
    } else if (a->tool == TOOL_WALL) {
        float dx = p.x - a->anchor.x;
        float dz = p.z - a->anchor.z;
        float len = sqrtf(dx * dx + dz * dz);
        float h = clampf(a->wall_h, MIN_HALF, 64.0f);
        float th = clampf(a->wall, MIN_HALF, 4.0f);

        line3(a, a->anchor, p, (unsigned char)'#', RGB(200, 220, 255));
        if (len > 0.001f) {
            draw_box_edges(a, v3((a->anchor.x + p.x) * 0.5f,
                                 a->anchor.y + h * 0.5f,
                                 (a->anchor.z + p.z) * 0.5f),
                           v3(len * 0.5f, h * 0.5f, th * 0.5f),
                           v3(0.0f, 0.0f, 0.0f), atan2f(-dz, dx),
                           RGB(200, 220, 255));
        }
    } else if (a->tool == TOOL_STAIRS) {
        line3(a, a->anchor, p, (unsigned char)'#', RGB(255, 220, 120));
    } else if (a->tool == TOOL_WATER) {
        float y = a->anchor.y;
        Vec3 lo = v3(a->anchor.x, y, a->anchor.z);
        Vec3 hi = v3(p.x, y, p.z);
        int gx;
        int gz;

        line3(a, v3(lo.x, y, lo.z), v3(hi.x, y, lo.z), (unsigned char)'~',
              RGB(120, 190, 255));
        line3(a, v3(hi.x, y, lo.z), v3(hi.x, y, hi.z), (unsigned char)'~',
              RGB(120, 190, 255));
        line3(a, v3(hi.x, y, hi.z), v3(lo.x, y, hi.z), (unsigned char)'~',
              RGB(120, 190, 255));
        line3(a, v3(lo.x, y, hi.z), v3(lo.x, y, lo.z), (unsigned char)'~',
              RGB(120, 190, 255));
        for (gz = 0; gz <= 6; gz++) {
            for (gx = 0; gx <= 6; gx++) {
                float fx = (float)gx / 6.0f;
                float fz = (float)gz / 6.0f;

                plot(a, v3(lo.x + (hi.x - lo.x) * fx, y,
                           lo.z + (hi.z - lo.z) * fz),
                     (unsigned char)'~', RGB(90, 150, 210));
            }
        }
    }
}

static void draw_panel(App *a)
{
    Ui *ui = &a->ui;
    int x = 1;
    int w = PANEL_W;
    int h = a->rows;
    int c = x + 2;
    int r = 2;
    int i;
    int mx;

    ui_panel(ui, x, 0, w, h, "iota editor");

    ui_label(ui, c, r++, "tool", RGB(214, 232, 255));
    for (i = 0; i < TOOL_COUNT; i++) {
        char tag[24];
        char key = i < TOOL_KEYS - 1 ? (char)('1' + i)
                 : i == TOOL_KEYS - 1 ? '0' : ' ';

        (void)snprintf(tag, sizeof tag, "%c %s", key, TOOL_NAME[i]);
        if (ui_button(ui, 100 + i, c, r++, tag)) {
            select_tool(a, (Tool)i);
        }
    }

    ui_label(ui, c, r++, "material", RGB(214, 232, 255));
    mx = c;
    for (i = 0; i < MAT_COUNT; i++) {
        if (ui_button(ui, 200 + i, mx, r, MATS[i].name)) {
            a->mat = i;
            if (a->sel >= 0 && a->sel < a->scene.obj_count) {
                a->scene.objs[a->sel].ink = MATS[i].ink;
                a->scene.objs[a->sel].flags = MATS[i].flags;
            }
        }
        mx += (int)strlen(MATS[i].name) + 3;
    }
    r++;

    ui_label(ui, c, r++, "shape", RGB(214, 232, 255));
    (void)ui_step(ui, 300, c, r++, w - 4, "riser", &a->riser, 0.05f, 0.1f,
                  1.0f, 2);
    (void)ui_step(ui, 301, c, r++, w - 4, "stairW", &a->stair_w, 0.1f, 0.4f,
                  6.0f, 1);
    (void)ui_step(ui, 303, c, r++, w - 4, "wall", &a->wall, 0.05f, 0.05f, 2.0f,
                  2);
    (void)ui_step(ui, 309, c, r++, w - 4, "wallH", &a->wall_h, 0.5f, 0.1f,
                  64.0f, 1);
    (void)ui_step(ui, 304, c, r++, w - 4, "depth", &a->depth, 0.5f, 0.5f, 64.0f,
                  1);
    (void)ui_step(ui, 305, c, r++, w - 4, "lamp", &a->lamp_i, 0.5f, 0.5f, 12.0f,
                  1);
    (void)ui_step(ui, 306, c, r++, w - 4, "res 0=auto", &a->res, 1.0f, 0.0f,
                  (float)VIEW_SS_MAX, 0);
    (void)ui_toggle(ui, 307, c, r, "grid", &a->grid);
    (void)ui_toggle(ui, 308, c + 9, r, "snap", &a->snap);
    r += 2;

    if (a->sel >= 0 && a->sel < a->scene.obj_count) {
        Obj *o = &a->scene.objs[a->sel];

        ui_label(ui, c, r++, "selected block", RGB(214, 232, 255));
        (void)ui_step(ui, 400, c, r++, w - 4, "x", &o->pos.x, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 401, c, r++, w - 4, "y", &o->pos.y, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 402, c, r++, w - 4, "z", &o->pos.z, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 403, c, r++, w - 4, "sx", &o->half.x, 0.1f, 0.05f,
                      32.0f, 1);
        (void)ui_step(ui, 404, c, r++, w - 4, "sy", &o->half.y, 0.1f, 0.05f,
                      32.0f, 1);
        (void)ui_step(ui, 405, c, r++, w - 4, "sz", &o->half.z, 0.1f, 0.05f,
                      32.0f, 1);
        (void)ui_step(ui, 409, c, r++, w - 4, "yaw", &o->yaw, EIGHTH, 0.0f, TAU,
                      2);
        if (ui_button(ui, 406, c, r, "turn-")) {
            o->yaw -= EIGHTH;
            if (o->yaw < 0.0f) {
                o->yaw += TAU;
            }
        }
        if (ui_button(ui, 407, c + 8, r, "turn+")) {
            o->yaw += EIGHTH;
            if (o->yaw > TAU) {
                o->yaw -= TAU;
            }
        }
        if (ui_button(ui, 408, c + 16, r, "cut")) {
            erase_at(a, a->sel);
        }
        r++;
        {
            float cr = (float)((o->ink >> 16) & 0xffu);
            float cg = (float)((o->ink >> 8) & 0xffu);
            float cb = (float)(o->ink & 0xffu);
            int ch = 0;

            ui_label(ui, c, r++, "colour", RGB(214, 232, 255));
            ch |= ui_step(ui, 410, c, r++, w - 4, "red", &cr, 1.0f, 0.0f,
                          255.0f, 0);
            ch |= ui_step(ui, 411, c, r++, w - 4, "grn", &cg, 1.0f, 0.0f,
                          255.0f, 0);
            ch |= ui_step(ui, 412, c, r++, w - 4, "blu", &cb, 1.0f, 0.0f,
                          255.0f, 0);
            if (ch != 0) {
                o->ink = ((unsigned)(cr + 0.5f) & 0xffu) << 16 |
                         ((unsigned)(cg + 0.5f) & 0xffu) << 8 |
                         ((unsigned)(cb + 0.5f) & 0xffu);
            }
            {
                int sx;

                for (sx = 0; sx < w - 4; sx++) {
                    render_put(ui->r, c + sx, r, (unsigned char)'=', o->ink);
                }
            }
            r++;
        }
    } else if (a->sel_light >= 0 && a->sel_light < a->scene.light_count) {
        Light *l = &a->scene.lights[a->sel_light];

        ui_label(ui, c, r++, "selected lamp", RGB(214, 232, 255));
        (void)ui_step(ui, 500, c, r++, w - 4, "x", &l->pos.x, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 501, c, r++, w - 4, "y", &l->pos.y, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 502, c, r++, w - 4, "z", &l->pos.z, 0.5f, -128.0f,
                      128.0f, 1);
        (void)ui_step(ui, 503, c, r++, w - 4, "power", &l->intensity, 0.5f, 0.0f,
                      20.0f, 1);
        if (ui_button(ui, 504, c, r, "cut")) {
            int k;

            for (k = a->sel_light; k + 1 < a->scene.light_count; k++) {
                a->scene.lights[k] = a->scene.lights[k + 1];
            }
            a->scene.light_count--;
            a->sel_light = -1;
            say(a, "took that lamp away");
        }
        r++;
    }

    if (r < h - 2 && a->msg[0] != '\0') {
        ui_label(ui, c, h - 2, a->msg, RGB(180, 230, 255));
    }
}

static void draw_help(App *a)
{
    static const char *const lines[] = {
        "h         toggle this help",
        "WASD      fly, space/c rise and sink",
        "arrows    look around",
        "click     build the current tool",
        "rclick    cancel the anchor, or take a panel",
        "1..9, 0   switch tool, tab for the next",
        "click num type a value, enter lands it",
        "m         flip the block material",
        "r         turn the block under the cursor",
        "g         toggle snapping to the half-metre",
        "u ctrl+z   undo, ctrl+y redo",
        "n         start a fresh level",
        "ctrl+s    write the level",
        "ctrl+o    read the level back",
        "esc       cancel, or quit when nothing is held"
    };
    int n = (int)(sizeof lines / sizeof lines[0]);
    int w = 54;
    int h = n + 2;
    int x = (a->cols - w) / 2;
    int y = (a->rows - h) / 2;
    int i;

    ui_panel(&a->ui, x, y, w, h, "keys");
    for (i = 0; i < n; i++) {
        render_text(a->render, x + 2, y + 1 + i, lines[i], RGB(180, 210, 240));
    }
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    ViewStats stats;
    Render *r = engine_render(e);
    int ss;

    refresh_basis(a);
    ss = a->res > 0.5f ? (int)(a->res + 0.5f) : engine_ss(e);
    view_render(&a->scene, &a->meshes, &a->water, &a->cam, r, ss, 0.0f, &stats);

    if (a->grid) {
        draw_grid(a);
    }
    draw_ghost(a);

    ui_begin(&a->ui, r, a->mx, a->my, a->click, a->btn_down, 0);
    draw_panel(a);
    if (a->help) {
        draw_help(a);
    }
    ui_end(&a->ui);

    render_put(r, a->mx, a->my, (unsigned char)'+', RGB(255, 255, 255));
    render_put(r, a->mx - 1, a->my, (unsigned char)'-', RGB(255, 255, 255));
    render_put(r, a->mx + 1, a->my, (unsigned char)'-', RGB(255, 255, 255));
    render_put(r, a->mx, a->my - 1, (unsigned char)'|', RGB(255, 255, 255));
    render_put(r, a->mx, a->my + 1, (unsigned char)'|', RGB(255, 255, 255));

    if (a->click && !a->ui.consumed) {
        ui_edit_stop(&a->ui);
        world_click(a);
    }
    if (a->rclick && !a->ui.consumed) {
        if (a->anchor_set) {
            a->anchor_set = 0;
            say(a, "cancelled");
        } else {
            const Obj *o = NULL;

            (void)cursor_point(a, &o, NULL);
            erase_under(a, o);
        }
    }
    a->click = 0;
    a->rclick = 0;
    a->ui.edit_commit = 0;
    hist_settle(a);

    {
        char line[160];

        (void)snprintf(line, sizeof line,
                       " h help   g snap   tab tool   %dx   %s%s   %s", ss,
                       a->path, a->scene.have_spawn ? "" : " (no spawn)",
                       TOOL_HINT[a->tool]);
        render_text(r, 0, a->rows - 1, line, RGB(96, 132, 160));
    }
}

int main(int argc, char **argv)
{
    EngineHooks hooks = {app_poll, app_sim, app_draw};
    App a;
    Engine *e;

    memset(&a, 0, sizeof a);
    input_clear(&a.input);
    a.mat = 1;
    a.tool = TOOL_BOX;
    a.snap = 1;
    a.grid = 1;
    a.sel = -1;
    a.sel_light = -1;
    a.riser = 0.25f;
    a.stair_w = 1.6f;
    a.wall = 0.25f;
    a.wall_h = 3.0f;
    a.depth = 2.0f;
    a.lamp_i = 4.0f;
    a.res = 0.0f;
    a.last_t = 12.0f;
    a.cam.fov = 1.05f;
    a.cam.pitch = -0.4f;
    a.cam.pos = v3(0.0f, 8.0f, 18.0f);
    (void)snprintf(a.path, sizeof a.path, "%s",
                   argc > 1 ? argv[1] : "edit.lvl");
    mesh_load(&a.meshes, "assets/gem.obj");
    if (level_load(&a.scene, a.path) != 0) {
        app_new(&a);
        (void)snprintf(a.msg, sizeof a.msg, "new level: %s", a.path);
    } else {
        water_apply(&a);
    }
    a.scratch = a.scene;
    hist_init(&a);
    e = engine_open(&hooks, &a);
    if (e == NULL) {
        (void)fprintf(stderr, "iota: no terminal to draw in\n");
        return 1;
    }
    a.render = engine_render(e);
    a.mx = render_cols(a.render) / 2;
    a.my = render_rows(a.render) / 2;
    term_mouse_enable(1);
    engine_run(e);
    engine_close(e);
    return 0;
}

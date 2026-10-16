#include "engine.h"
#include "input.h"
#include "level.h"
#include "render.h"
#include "term.h"
#include "view.h"
#include "water.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLY_SPEED 9.0f
#define LOOK_SPEED 2.0f
#define PITCH_MAX 1.5f
#define REACH 200.0f
#define STEP_HEIGHT 0.25f
#define STAIR_WIDTH 1.6f
#define MIN_HALF 0.05f

typedef enum {
    TOOL_BOX,
    TOOL_WATER,
    TOOL_STAIRS,
    TOOL_SPAWN,
    TOOL_LIGHT,
    TOOL_PAINT,
    TOOL_ERASE,
    TOOL_COUNT
} Tool;

static const char *const TOOL_NAME[TOOL_COUNT] = {
    "box", "water", "stairs", "spawn", "light", "paint", "erase"
};

static const char *const TOOL_HINT[TOOL_COUNT] = {
    "click two corners of a solid",
    "click the level and a corner, then the far corner",
    "click the foot, then the top",
    "click where the body starts",
    "click where a lamp hangs",
    "click a block to turn its material over",
    "click a panel to take it away"
};

typedef struct {
    const char *name;
    unsigned ink;
    unsigned flags;
} Material;

static const Material MATS[] = {
    {"wall", RGB(150, 150, 152), 0u},
    {"tile", RGB(96, 100, 116), OBJ_TILE},
    {"mirror", RGB(84, 92, 112), OBJ_MIRROR}
};

#define MAT_COUNT ((int)(sizeof MATS / sizeof MATS[0]))

typedef struct {
    Scene scene;
    Meshes meshes;
    Water water;
    Camera cam;
    Input input;
    Render *render;
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
    int anchor_set;
    Vec3 anchor;
    float last_t;
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

// one of the two points the box tool spans; the ray lands on whatever the
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

static void place_box(App *a, Vec3 lo, Vec3 hi)
{
    Vec3 c;
    Vec3 h;
    Obj *o;

    box_corners(lo, hi, &c, &h);
    h = v3(clampf(h.x, MIN_HALF, 64.0f), clampf(h.y, MIN_HALF, 64.0f),
           clampf(h.z, MIN_HALF, 64.0f));
    o = scene_add_obj(&a->scene);
    if (o == NULL) {
        say(a, "no room left for another panel");
        return;
    }
    o->kind = OBJ_BOX;
    o->pos = c;
    o->half = h;
    o->ink = MATS[a->mat].ink;
    o->flags = MATS[a->mat].flags;
    (void)snprintf(a->msg, sizeof a->msg, "laid a %s block",
                   MATS[a->mat].name);
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
    n = (int)lroundf(rise / STEP_HEIGHT);
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
        Obj *o = scene_add_obj(&a->scene);

        if (o == NULL) {
            break;
        }
        if (fabsf(dx) >= fabsf(dz)) {
            float dir = dx >= 0.0f ? 1.0f : -1.0f;

            o->pos = v3(lo.x + dir * run * ((float)i + 0.5f),
                        (lo.y + top) * 0.5f, lo.z);
            o->half = v3(run * 0.5f, (top - lo.y) * 0.5f, STAIR_WIDTH * 0.5f);
        } else {
            float dir = dz >= 0.0f ? 1.0f : -1.0f;

            o->pos = v3(lo.x, (lo.y + top) * 0.5f,
                        lo.z + dir * run * ((float)i + 0.5f));
            o->half = v3(STAIR_WIDTH * 0.5f, (top - lo.y) * 0.5f, run * 0.5f);
        }
        o->kind = OBJ_BOX;
        o->ink = MATS[a->mat].ink;
    }
    (void)snprintf(a->msg, sizeof a->msg, "laid %d steps up", n);
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
    l->intensity = 4.0f;
    say(a, "hung a lamp");
}

static void cycle_material(Obj *o)
{
    if ((o->flags & OBJ_MIRROR) != 0u) {
        o->flags = 0u;
        o->ink = MATS[0].ink;
    } else if ((o->flags & OBJ_TILE) != 0u) {
        o->flags = OBJ_MIRROR;
        o->ink = MATS[2].ink;
    } else {
        o->flags = OBJ_TILE;
        o->ink = MATS[1].ink;
    }
}

static void paint_under(App *a, const Obj *obj)
{
    const char *name;

    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    cycle_material((Obj *)obj);
    name = (obj->flags & OBJ_MIRROR) != 0u ? MATS[2].name
           : (obj->flags & OBJ_TILE) != 0u ? MATS[1].name
                                           : MATS[0].name;
    (void)snprintf(a->msg, sizeof a->msg, "now a %s block", name);
}

static void rotate_under(App *a)
{
    const Obj *obj = NULL;

    (void)cursor_point(a, &obj, NULL);
    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    ((Obj *)obj)->yaw += 0.785398163f;
    if (((Obj *)obj)->yaw > 6.283185307f) {
        ((Obj *)obj)->yaw -= 6.283185307f;
    }
    say(a, "turned an eighth of a turn");
}

static void erase_under(App *a, const Obj *obj)
{
    int idx;
    int i;

    if (obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    idx = (int)(obj - a->scene.objs);
    for (i = idx; i + 1 < a->scene.obj_count; i++) {
        a->scene.objs[i] = a->scene.objs[i + 1];
    }
    a->scene.obj_count--;
    say(a, "took that panel away");
}

static void click(App *a)
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
    } else if (a->tool == TOOL_WATER) {
        place_water(a, a->anchor, p);
    } else if (a->tool == TOOL_STAIRS) {
        place_stairs(a, a->anchor, p);
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
    l->intensity = 5.0f;
    a->scene.spawn = v3(0.0f, 0.0f, 0.0f);
    a->scene.have_spawn = 1;
    water_apply(a);
}

static void app_poll(Engine *e)
{
    App *a = engine_user(e);
    int k;

    refresh_basis(a);
    while ((k = term_read_key()) != KEY_NONE) {
        double now = engine_time(e);
        int lc;

        if (k == KEY_MOUSE) {
            a->mx = term_mouse_x();
            a->my = term_mouse_y();
            if (term_mouse_motion() || !term_mouse_press()) {
                continue;
            }
            if (term_mouse_button() == 0) {
                click(a);
            } else if (term_mouse_button() == 2) {
                if (a->anchor_set) {
                    a->anchor_set = 0;
                    say(a, "cancelled");
                } else {
                    const Obj *o = NULL;

                    (void)cursor_point(a, &o, NULL);
                    erase_under(a, o);
                }
            }
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
                (void)snprintf(a->msg, sizeof a->msg, "read %s", a->path);
            } else {
                say(a, "could not read the level");
            }
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            a->tool = (Tool)(lc - '1');
            a->anchor_set = 0;
            (void)snprintf(a->msg, sizeof a->msg, "%s: %s", TOOL_NAME[a->tool],
                           TOOL_HINT[a->tool]);
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
            a->tool = TOOL_SPAWN;
            click(a);
            break;
        case 'u':
            if (a->scene.obj_count > 0) {
                a->scene.obj_count--;
                say(a, "undone");
            }
            break;
        case 'n':
            app_new(a);
            say(a, "a fresh level");
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

static void draw_toolbar(App *a)
{
    Render *r = a->render;
    int x = 1;
    int i;

    for (i = 0; i < TOOL_COUNT; i++) {
        unsigned ink = (Tool)i == a->tool ? RGB(255, 232, 150) : RGB(88, 118, 150);
        char tag[16];

        (void)snprintf(tag, sizeof tag, "%d %s", i + 1, TOOL_NAME[i]);
        render_text(r, x, 0, tag, ink);
        x += (int)strlen(tag) + 2;
    }
    x = 1;
    for (i = 0; i < MAT_COUNT; i++) {
        unsigned ink = i == a->mat ? RGB(255, 232, 150) : RGB(88, 118, 150);

        render_text(r, x, 1, MATS[i].name, ink);
        x += (int)strlen(MATS[i].name) + 2;
    }
    render_text(r, x, 1, a->snap ? "[snap]" : " snap ", RGB(120, 160, 190));
}

static void draw_ghost(App *a)
{
    const Obj *obj = NULL;
    Vec3 p = cursor_point(a, &obj, NULL);

    if (a->tool == TOOL_ERASE) {
        if (obj != NULL) {
            draw_box_edges(a, obj->pos, obj->half, v3(0.04f, 0.04f, 0.04f),
                           obj->yaw, RGB(255, 110, 110));
        }
        return;
    }
    if (a->tool == TOOL_SPAWN) {
        draw_box_edges(a, v3(p.x, p.y + 0.2f, p.z), v3(0.3f, 0.2f, 0.3f),
                       v3(0.0f, 0.0f, 0.0f), 0.0f, RGB(120, 240, 140));
        line3(a, p, v3(p.x, p.y + 1.4f, p.z), (unsigned char)'|',
              RGB(120, 240, 140));
        return;
    }
    if (a->tool == TOOL_LIGHT) {
        line3(a, v3(p.x - 0.4f, p.y, p.z), v3(p.x + 0.4f, p.y, p.z),
              (unsigned char)'-', RGB(255, 230, 150));
        line3(a, v3(p.x, p.y, p.z - 0.4f), v3(p.x, p.y, p.z + 0.4f),
              (unsigned char)'-', RGB(255, 230, 150));
        line3(a, v3(p.x, p.y - 0.4f, p.z), v3(p.x, p.y + 0.4f, p.z),
              (unsigned char)'|', RGB(255, 230, 150));
        return;
    }

    if (!a->anchor_set) {
        draw_box_edges(a, p, v3(0.12f, 0.12f, 0.12f), v3(0.0f, 0.0f, 0.0f),
                       0.0f, RGB(255, 236, 140));
        return;
    }

    draw_box_edges(a, a->anchor, v3(0.1f, 0.1f, 0.1f), v3(0.0f, 0.0f, 0.0f),
                   0.0f, RGB(120, 220, 255));

    if (a->tool == TOOL_BOX) {
        Vec3 c;
        Vec3 h;

        box_corners(a->anchor, p, &c, &h);
        draw_box_edges(a, c, h, v3(0.0f, 0.0f, 0.0f), 0.0f, RGB(255, 236, 140));
    } else if (a->tool == TOOL_STAIRS) {
        line3(a, a->anchor, p, (unsigned char)'#', RGB(255, 220, 120));
    } else if (a->tool == TOOL_WATER) {
        float y = a->anchor.y;
        Vec3 lo = v3(a->anchor.x, y, a->anchor.z);
        Vec3 hi = v3(p.x, y, p.z);
        Vec3 a0;
        Vec3 a1;
        Vec3 a2;
        Vec3 a3;
        int gx;
        int gz;

        a0 = v3(lo.x, y, lo.z);
        a1 = v3(hi.x, y, lo.z);
        a2 = v3(hi.x, y, hi.z);
        a3 = v3(lo.x, y, hi.z);
        line3(a, a0, a1, (unsigned char)'~', RGB(120, 190, 255));
        line3(a, a1, a2, (unsigned char)'~', RGB(120, 190, 255));
        line3(a, a2, a3, (unsigned char)'~', RGB(120, 190, 255));
        line3(a, a3, a0, (unsigned char)'~', RGB(120, 190, 255));
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

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    ViewStats stats;
    Render *r = engine_render(e);
    const Obj *o = NULL;
    Vec3 p;
    char line[160];

    refresh_basis(a);
    view_render(&a->scene, &a->meshes, &a->water, &a->cam, r, 1, 0.0f, &stats);

    draw_ghost(a);

    {
        unsigned ink = RGB(255, 255, 255);
        int cx = a->mx;
        int cy = a->my;

        render_put(r, cx, cy, (unsigned char)'+', ink);
        render_put(r, cx - 1, cy, (unsigned char)'-', ink);
        render_put(r, cx + 1, cy, (unsigned char)'-', ink);
        render_put(r, cx, cy - 1, (unsigned char)'|', ink);
        render_put(r, cx, cy + 1, (unsigned char)'|', ink);
    }

    p = cursor_point(a, &o, NULL);
    draw_toolbar(a);
    (void)snprintf(line, sizeof line, " %-7s %s   at %.1f %.1f %.1f   %s",
                   TOOL_NAME[a->tool], TOOL_HINT[a->tool], (double)p.x,
                   (double)p.y, (double)p.z, a->msg);
    render_text(r, 0, 2, line, RGB(180, 230, 255));
    (void)snprintf(line, sizeof line, " %s  %s", a->path,
                   a->scene.have_spawn ? "spawn set" : "no spawn");
    render_text(r, 0, 3, line, RGB(140, 170, 200));
    render_text(r, 0, a->rows - 1,
                " WASD fly  space/c up-down  click build  rclick cancel/take"
                "  1-7 tool  m material  r turn  g snap  u undo"
                "  Ctrl+S/O save load  n new  Esc quit",
                RGB(96, 132, 160));
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

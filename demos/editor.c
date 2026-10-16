#include "engine.h"
#include "input.h"
#include "level.h"
#include "render.h"
#include "term.h"
#include "view.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLY_SPEED 9.0f
#define LOOK_SPEED 2.0f
#define PITCH_MAX 1.5f
#define REACH 160.0f
#define STEP_HEIGHT 0.25f
#define STAIR_WIDTH 1.6f
#define SIZE_MIN 0.1f
#define SIZE_MAX 8.0f
#define THICK_MIN 0.02f
#define THICK_MAX 1.0f

typedef struct {
    const char *name;
    unsigned ink;
    unsigned flags;
} Panel;

static const Panel PANELS[] = {
    {"wall", RGB(122, 128, 142), 0u},
    {"tile", RGB(96, 100, 116), OBJ_TILE},
    {"mirror", RGB(76, 84, 104), OBJ_MIRROR}
};

#define PANEL_COUNT ((int)(sizeof PANELS / sizeof PANELS[0]))

typedef struct {
    Scene scene;
    Meshes meshes;
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
    float size;
    float thick;
    int mat;
    int stair_arm;
    Vec3 stair_a;
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

static void line3(App *a, Vec3 p, Vec3 q, unsigned char ch, unsigned ink)
{
    int steps = 20;
    int i;

    for (i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        Vec3 pnt = v3_add(p, v3_mul(v3_sub(q, p), t));
        int cx;
        int cy;

        if (to_cell(a, pnt, &cx, &cy)) {
            render_put(a->render, cx, cy, ch, ink);
        }
    }
}

static int face_axis(Vec3 n)
{
    if (fabsf(n.x) >= fabsf(n.y) && fabsf(n.x) >= fabsf(n.z)) {
        return 0;
    }
    if (fabsf(n.y) >= fabsf(n.z)) {
        return 1;
    }
    return 2;
}

static void panel_box(const App *a, Vec3 point, Vec3 n, Vec3 *c, Vec3 *half)
{
    int axis = face_axis(n);
    float s = a->size;
    float t = a->thick;

    *c = v3_add(point, v3_mul(n, t));
    if (axis == 0) {
        c->y = roundf(c->y);
        c->z = roundf(c->z);
        *half = v3(t, s, s);
    } else if (axis == 1) {
        c->x = roundf(c->x);
        c->z = roundf(c->z);
        *half = v3(s, t, s);
    } else {
        c->x = roundf(c->x);
        c->y = roundf(c->y);
        *half = v3(s, s, t);
    }
}

static void draw_box_edges(App *a, Vec3 c, Vec3 h, unsigned ink)
{
    Vec3 p[8];
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
                p[i] = v3(c.x + h.x * (float)sx, c.y + h.y * (float)sy,
                          c.z + h.z * (float)sz);
                i++;
            }
        }
    }
    for (i = 0; i < 12; i++) {
        line3(a, p[e[i][0]], p[e[i][1]], (unsigned char)'+', ink);
    }
}

static int pick(const App *a, Hit *hit)
{
    Vec3 dir = mouse_dir(a);

    *hit = cast_ray(&a->scene, &a->meshes, a->cam.pos, dir, REACH);
    return hit->action != HIT_NONE;
}

static void place_panel(App *a, Vec3 point, Vec3 n)
{
    Vec3 c;
    Vec3 half;
    Obj *o;

    panel_box(a, point, n, &c, &half);
    o = scene_add_obj(&a->scene);
    if (o == NULL) {
        say(a, "no room left for another panel");
        return;
    }
    o->kind = OBJ_BOX;
    o->pos = c;
    o->half = half;
    o->ink = PANELS[a->mat].ink;
    o->flags = PANELS[a->mat].flags;
    (void)snprintf(a->msg, sizeof a->msg, "placed a %s panel",
                   PANELS[a->mat].name);
}

static void build_stairs(App *a, Vec3 lo, Vec3 hi)
{
    float dx = hi.x - lo.x;
    float dz = hi.z - lo.z;
    float run;
    float sh;
    float rise = hi.y - lo.y;
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

            o->pos = v3(lo.x + dir * run * ((float)i + 0.5f), (lo.y + top) * 0.5f,
                        lo.z);
            o->half = v3(run * 0.5f, (top - lo.y) * 0.5f, STAIR_WIDTH * 0.5f);
        } else {
            float dir = dz >= 0.0f ? 1.0f : -1.0f;

            o->pos = v3(lo.x, (lo.y + top) * 0.5f,
                        lo.z + dir * run * ((float)i + 0.5f));
            o->half = v3(STAIR_WIDTH * 0.5f, (top - lo.y) * 0.5f, run * 0.5f);
        }
        o->kind = OBJ_BOX;
        o->ink = RGB(150, 140, 124);
    }
    (void)snprintf(a->msg, sizeof a->msg, "laid %d steps up", n);
}

static void click_scene(App *a)
{
    Hit hit;

    if (!pick(a, &hit)) {
        say(a, "nothing under the cursor");
        return;
    }
    if (a->stair_arm == 1) {
        a->stair_a = hit.point;
        a->stair_arm = 2;
        say(a, "foot of the stairs set, click the top");
        return;
    }
    if (a->stair_arm == 2) {
        build_stairs(a, a->stair_a, hit.point);
        a->stair_arm = 0;
        return;
    }
    place_panel(a, hit.point, hit.normal);
}

static void remove_under_cursor(App *a)
{
    Hit hit;

    if (!pick(a, &hit) || hit.obj == NULL) {
        say(a, "nothing under the cursor");
        return;
    }
    {
        int idx = (int)(hit.obj - a->scene.objs);
        int i;

        for (i = idx; i + 1 < a->scene.obj_count; i++) {
            a->scene.objs[i] = a->scene.objs[i + 1];
        }
        a->scene.obj_count--;
        say(a, "took that panel away");
    }
}

static void set_spawn(App *a)
{
    Hit hit;

    if (!pick(a, &hit)) {
        say(a, "aim at the ground to set the spawn");
        return;
    }
    a->scene.spawn = hit.point;
    a->scene.have_spawn = 1;
    say(a, "spawn set");
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
    o->ink = RGB(96, 100, 116);
    o->flags = OBJ_TILE;
    l = scene_add_light(&a->scene);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 6.0f, 0.0f);
    l->color = v3(1.0f, 0.96f, 0.9f);
    l->intensity = 5.0f;
    a->scene.spawn = v3(0.0f, 0.0f, 0.0f);
    a->scene.have_spawn = 1;
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
            if (term_mouse_wheel() != 0) {
                a->size = clampf(a->size + 0.25f * (float)term_mouse_wheel(),
                                 SIZE_MIN, SIZE_MAX);
                continue;
            }
            if (term_mouse_button() == 0) {
                click_scene(a);
            } else if (term_mouse_button() == 2) {
                remove_under_cursor(a);
            }
            continue;
        }
        lc = lower(k);
        input_arrive(&a->input, lc, now);
        switch (lc) {
        case KEY_ESC:
        case 3:
            engine_quit(e);
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
                (void)snprintf(a->msg, sizeof a->msg, "read %s", a->path);
            } else {
                say(a, "could not read the level");
            }
            break;
        case '1':
        case '2':
        case '3':
            if (lc - '1' < PANEL_COUNT) {
                a->mat = lc - '1';
                (void)snprintf(a->msg, sizeof a->msg, "%s panel",
                               PANELS[a->mat].name);
            }
            break;
        case '[':
            a->size = clampf(a->size - 0.25f, SIZE_MIN, SIZE_MAX);
            break;
        case ']':
            a->size = clampf(a->size + 0.25f, SIZE_MIN, SIZE_MAX);
            break;
        case '-':
            a->thick = clampf(a->thick - 0.02f, THICK_MIN, THICK_MAX);
            break;
        case '=':
            a->thick = clampf(a->thick + 0.02f, THICK_MIN, THICK_MAX);
            break;
        case 'p':
            set_spawn(a);
            break;
        case 't':
            a->stair_arm = a->stair_arm == 0 ? 1 : 0;
            say(a, a->stair_arm ? "stairs: click the foot" : "stairs off");
            break;
        case 'x':
            remove_under_cursor(a);
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
        a->cam.pos = v3_add(a->cam.pos,
                            v3_mul(v3_norm(wish), FLY_SPEED * dt));
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
}

static void draw_status(App *a, char *line, size_t n)
{
    (void)snprintf(line, n,
                   " %s  size %.2f  thick %.2f  panel %d/%d%s  spawn %s  %s",
                   a->path, (double)a->size, (double)a->thick, a->mat + 1,
                   PANEL_COUNT, a->stair_arm ? "  STAIRS" : "",
                   a->scene.have_spawn ? "set" : "-", a->msg);
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    ViewStats stats;
    Render *r = engine_render(e);
    Hit hit;
    int have = 0;
    char line[160];

    refresh_basis(a);
    view_render(&a->scene, &a->meshes, NULL, &a->cam, r, 1, 0.0f, &stats);

    if (a->stair_arm == 2) {
        line3(a, a->stair_a, a->stair_a, (unsigned char)'*', RGB(255, 220, 90));
    } else if (pick(a, &hit)) {
        Vec3 c;
        Vec3 half;

        have = 1;
        panel_box(a, hit.point, hit.normal, &c, &half);
        draw_box_edges(a, c, half, RGB(255, 236, 140));
    }

    if (a->scene.have_spawn) {
        Vec3 s = a->scene.spawn;

        draw_box_edges(a, v3(s.x, s.y + 0.15f, s.z), v3(0.35f, 0.15f, 0.35f),
                       RGB(120, 240, 140));
        line3(a, s, v3(s.x, s.y + 1.2f, s.z), (unsigned char)'|',
              RGB(120, 240, 140));
    }

    {
        int cx = a->mx;
        int cy = a->my;
        unsigned ink = have ? RGB(255, 255, 255) : RGB(150, 90, 90);

        render_put(r, cx, cy, (unsigned char)'+', ink);
        render_put(r, cx - 1, cy, (unsigned char)'-', ink);
        render_put(r, cx + 1, cy, (unsigned char)'-', ink);
        render_put(r, cx, cy - 1, (unsigned char)'|', ink);
        render_put(r, cx, cy + 1, (unsigned char)'|', ink);
    }

    draw_status(a, line, sizeof line);
    render_text(r, 0, 0, line, RGB(180, 230, 255));
    render_text(r, 0, 1,
                " WASD fly  space/c up-down  arrows look  click place  rclick del"
                "  1-3 panel  [ ] size  - = thick  t stairs  p spawn  u undo",
                RGB(96, 132, 160));
    render_text(r, 0, 2,
                " Ctrl+S save  Ctrl+O load  n new  Esc quit",
                RGB(96, 132, 160));
}

int main(int argc, char **argv)
{
    EngineHooks hooks = {app_poll, app_sim, app_draw};
    App a;
    Engine *e;

    memset(&a, 0, sizeof a);
    input_clear(&a.input);
    a.size = 1.0f;
    a.thick = 0.06f;
    a.mat = 1;
    a.cam.fov = 1.05f;
    a.cam.pitch = -0.35f;
    a.cam.pos = v3(0.0f, 6.0f, 14.0f);
    (void)snprintf(a.path, sizeof a.path, "%s",
                   argc > 1 ? argv[1] : "edit.lvl");
    mesh_load(&a.meshes, "assets/gem.obj");
    if (level_load(&a.scene, a.path) != 0) {
        app_new(&a);
        (void)snprintf(a.msg, sizeof a.msg, "new level: %s", a.path);
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

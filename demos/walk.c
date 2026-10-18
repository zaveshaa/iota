#include "engine.h"
#include "input.h"
#include "level.h"
#include "view.h"
#include "walk.h"
#include "water.h"
#include "world.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EYE 1.6f
#define SPEED 6.0f
#define TURN_SPEED 2.0f
#define PITCH_SPEED 1.3f
#define PITCH_MAX 1.4f
#define MAP_MAX 64
#define MAP_NAME 256

typedef struct {
    Scene scene;
    Meshes meshes;
    Water water;
    Camera cam;
    Walk player;
    ViewStats stats;
    Input input;
    int want_jump;
    Vec3 prev_pos;
    float prev_yaw;
    float prev_pitch;
    float source_acc;
    int gem;
    char maps[MAP_MAX][MAP_NAME];
    int map_count;
    int cur;
    char label[MAP_NAME];
} App;

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static Obj *add_box(Scene *s, Vec3 pos, Vec3 half, unsigned ink)
{
    Obj *o = scene_add_obj(s);

    o->kind = OBJ_BOX;
    o->pos = pos;
    o->half = half;
    o->ink = ink;
    return o;
}

static void build_range(Scene *s, int gem)
{
    int i;

    scene_clear(s);
    /* a moon hangs over the range so the sky shows its stars and its lamp
       together, and the tile and the pool catch the soft light the shadows
       leave between them */
    s->have_moon = 1;
    s->sky_moon_dir = v3(-0.28f, 0.30f, 1.0f);
    s->sky_moon = RGB(232, 230, 208);
    s->sky_moon_size = 0.08f;
    /* the ground is a frame of four slabs that leaves an open square for the
       pit, so the main plane does not bury it */
    add_box(s, v3(-6.0f, -0.5f, 0.0f), v3(14.0f, 0.5f, 20.0f),
            RGB(96, 100, 116))->flags = OBJ_TILE;
    add_box(s, v3(18.0f, -0.5f, 0.0f), v3(2.0f, 0.5f, 20.0f),
            RGB(96, 100, 116))->flags = OBJ_TILE;
    add_box(s, v3(12.0f, -0.5f, -14.5f), v3(4.0f, 0.5f, 5.5f),
            RGB(96, 100, 116))->flags = OBJ_TILE;
    add_box(s, v3(12.0f, -0.5f, 9.5f), v3(4.0f, 0.5f, 10.5f),
            RGB(96, 100, 116))->flags = OBJ_TILE;

    for (i = 0; i < 6; i++) {
        float h = 0.25f * (float)(i + 1);

        add_box(s, v3(-4.0f, h * 0.5f, 4.0f - 0.5f * (float)i),
                v3(1.5f, h * 0.5f, 0.25f), RGB(160, 150, 130));
    }

    add_box(s, v3(0.0f, 0.5f, 8.5f), v3(8.0f, 2.5f, 0.25f),
            RGB(76, 84, 104))->flags = OBJ_MIRROR;
    add_box(s, v3(-2.4f, 0.75f, 2.0f), v3(0.5f, 0.75f, 0.5f),
            RGB(224, 144, 64));
    add_box(s, v3(2.4f, 0.75f, 2.0f), v3(0.5f, 0.75f, 0.5f),
            RGB(70, 180, 220));

    /* a square pit in the corner, its floor well below ground and its water
       filled to half depth; a short stair leads down into it */
    add_box(s, v3(12.0f, -1.7f, -5.0f), v3(4.0f, 0.1f, 4.0f), RGB(84, 90, 108));
    add_box(s, v3(12.0f, -0.8f, -0.95f), v3(4.0f, 0.8f, 0.05f),
            RGB(94, 100, 116));
    add_box(s, v3(12.0f, -0.8f, -9.05f), v3(4.0f, 0.8f, 0.05f),
            RGB(94, 100, 116));
    add_box(s, v3(7.95f, -0.8f, -5.0f), v3(0.05f, 0.8f, 4.05f),
            RGB(94, 100, 116));
    add_box(s, v3(16.05f, -0.8f, -5.0f), v3(0.05f, 0.8f, 4.05f),
            RGB(94, 100, 116));
    add_box(s, v3(12.0f, -0.6f, -1.5f), v3(1.5f, 0.2f, 0.5f),
            RGB(110, 116, 132));
    add_box(s, v3(12.0f, -1.0f, -2.0f), v3(1.5f, 0.2f, 0.5f),
            RGB(110, 116, 132));
    add_box(s, v3(12.0f, -1.4f, -2.5f), v3(1.5f, 0.2f, 0.5f),
            RGB(110, 116, 132));

    if (gem >= 0) {
        Obj *o = scene_add_obj(s);

        o->kind = OBJ_MESH;
        o->pos = v3(0.0f, 5.0f, 0.0f);
        o->ink = RGB(220, 130, 230);
        o->mesh = gem;
    } else {
        Obj *o = scene_add_obj(s);

        o->kind = OBJ_SPHERE;
        o->pos = v3(3.0f, 0.7f, 5.0f);
        o->half = v3(0.7f, 0.7f, 0.7f);
        o->ink = RGB(220, 220, 220);
    }

    {
        Light *l = scene_add_light(s);

        l->kind = LIGHT_POINT;
        l->pos = v3(0.0f, 4.0f, 4.0f);
        l->color = v3(1.0f, 0.97f, 0.92f);
        l->intensity = 4.0f;
    }
    {
        Light *l = scene_add_light(s);

        l->kind = LIGHT_POINT;
        l->pos = v3(-4.8f, 2.0f, 3.2f);
        l->color = v3(0.85f, 0.95f, 1.0f);
        l->intensity = 2.0f;
    }
    {
        Light *l = scene_add_light(s);

        l->kind = LIGHT_POINT;
        l->pos = v3(12.0f, 2.4f, -5.0f);
        l->color = v3(0.80f, 0.95f, 1.0f);
        l->intensity = 2.5f;
    }
    {
        Light *l = scene_add_light(s);

        l->kind = LIGHT_POINT;
        l->pos = v3(2.4f, 3.4f, 2.0f);
        l->color = v3(1.0f, 0.92f, 0.80f);
        l->intensity = 3.0f;
    }
}

static int cmp_map(const void *x, const void *y)
{
    return strcmp((const char *)x, (const char *)y);
}

// the levels reachable from here, so Tab can walk from one to the next: first
// the working directory, then a levels/ folder when one is there
static void scan_dir(char maps[MAP_MAX][MAP_NAME], int *n, const char *dir,
                     const char *prefix)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    size_t pl = strlen(prefix);

    if (d == NULL) {
        return;
    }
    while ((de = readdir(d)) != NULL && *n < MAP_MAX) {
        size_t len = strlen(de->d_name);

        if (de->d_name[0] == '.' || len < 4 || len >= (size_t)MAP_NAME) {
            continue;
        }
        if (strcmp(de->d_name + len - 4, ".lvl") != 0) {
            continue;
        }
        if (pl + len >= (size_t)MAP_NAME) {
            continue;
        }
        memcpy(maps[*n], prefix, pl);
        memcpy(maps[*n] + pl, de->d_name, len + 1);
        (*n)++;
    }
    closedir(d);
}

static int scan_maps(char maps[MAP_MAX][MAP_NAME])
{
    int n = 0;

    scan_dir(maps, &n, ".", "");
    scan_dir(maps, &n, "levels", "levels/");
    qsort(maps, (size_t)n, MAP_NAME, cmp_map);
    return n;
}

// slot 0 is the built-in range, slot k is maps[k-1]; the body restarts at the
// map's spawn and whatever water it holds replaces the last pool
static void load_slot(App *a, int slot)
{
    a->cur = slot;
    if (slot <= 0) {
        build_range(&a->scene, a->gem);
        water_open(&a->water, v3(7.9f, 0.0f, -9.1f), v3(16.1f, 0.0f, -0.9f),
                   -0.8f, 0.5f);
        water_spout(&a->water, v3(10.0f, 2.2f, -3.0f), 1.1f);
        water_spout(&a->water, v3(8.6f, 2.0f, -6.6f), 0.8f);
        water_spout(&a->water, v3(13.8f, 2.4f, -4.2f), 0.9f);
        water_spout(&a->water, v3(12.0f, 2.8f, -1.4f), 0.6f);
        (void)snprintf(a->label, sizeof a->label, "the test range");
    } else {
        const char *path = a->maps[slot - 1];

        if (level_load(&a->scene, path) != 0) {
            (void)snprintf(a->label, sizeof a->label, "%s - cannot read", path);
            return;
        }
        if (a->scene.have_water) {
            int i;

            water_open(&a->water, a->scene.water.lo, a->scene.water.hi,
                       a->scene.water.level, a->scene.water.spacing);
            for (i = 0; i < a->scene.water.spout_count; i++) {
                water_spout(&a->water, a->scene.water.spouts[i].top,
                            a->scene.water.spouts[i].rate);
            }
        } else {
            water_close(&a->water);
        }
        (void)snprintf(a->label, sizeof a->label, "%s", path);
    }
    a->player.pos = a->scene.have_spawn ? a->scene.spawn : v3(0.0f, 0.0f, -3.0f);
    a->player.vel = v3(0.0f, 0.0f, 0.0f);
    a->player.jump = 0;
    a->player.coyote = 0.0f;
    a->prev_pos = a->player.pos;
    a->cam.pos = v3(a->player.pos.x, a->player.pos.y + EYE, a->player.pos.z);
    a->prev_yaw = a->cam.yaw;
    a->prev_pitch = a->cam.pitch;
}

static void app_poll(Engine *e)
{
    App *a = engine_user(e);
    int k;

    while ((k = term_read_key()) != KEY_NONE) {
        int lc = (k >= 'A' && k <= 'Z') ? k + 32 : k;
        double now = engine_time(e);

        input_arrive(&a->input, lc, now);
        switch (lc) {
        case KEY_ESC:
        case 3:
            engine_quit(e);
            break;
        case ' ':
            if (!term_key_released()) {
                a->want_jump = 1;
            }
            break;
        case 9:
            load_slot(a, a->cur + 1 > a->map_count ? 0 : a->cur + 1);
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
    Vec3 fwd = v3(sinf(a->cam.yaw), 0.0f, cosf(a->cam.yaw));
    Vec3 right = v3(-cosf(a->cam.yaw), 0.0f, sinf(a->cam.yaw));
    Vec3 wish = v3(0.0f, 0.0f, 0.0f);

    a->prev_pos = a->player.pos;
    a->prev_yaw = a->cam.yaw;
    a->prev_pitch = a->cam.pitch;

    if (input_held(&a->input, 'w', now)) {
        wish = v3_add(wish, fwd);
    }
    if (input_held(&a->input, 's', now)) {
        wish = v3_sub(wish, fwd);
    }
    if (input_held(&a->input, 'd', now)) {
        wish = v3_add(wish, right);
    }
    if (input_held(&a->input, 'a', now)) {
        wish = v3_sub(wish, right);
    }
    if (a->want_jump) {
        a->player.jump = 1;
        a->want_jump = 0;
    }
    if (input_held(&a->input, KEY_LEFT, now)) {
        a->cam.yaw += TURN_SPEED * dt;
    }
    if (input_held(&a->input, KEY_RIGHT, now)) {
        a->cam.yaw -= TURN_SPEED * dt;
    }
    if (input_held(&a->input, KEY_UP, now)) {
        a->cam.pitch += PITCH_SPEED * dt;
    }
    if (input_held(&a->input, KEY_DOWN, now)) {
        a->cam.pitch -= PITCH_SPEED * dt;
    }
    if (a->cam.pitch > PITCH_MAX) {
        a->cam.pitch = PITCH_MAX;
    }
    if (a->cam.pitch < -PITCH_MAX) {
        a->cam.pitch = -PITCH_MAX;
    }

    walk_move(&a->scene, &a->meshes, &a->player, wish, SPEED, dt);
    {
        Vec3 feet = a->player.pos;
        int wet = water_body(&a->water, feet, a->player.radius, a->player.vel,
                             dt);
        float w = wet ? 1.0f : 0.0f;
        float k;

        k = 1.0f - 0.8f * w * dt;
        a->player.vel.x *= k;
        a->player.vel.z *= k;
        water_step(&a->water, dt);
    }
    a->cam.pos = v3(a->player.pos.x, a->player.pos.y + EYE, a->player.pos.z);
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    Render *r = engine_render(e);
    Camera view = a->cam;
    float alpha = (float)engine_alpha(e);
    char line[192];

    view.pos = v3(lerpf(a->prev_pos.x, a->player.pos.x, alpha),
                  lerpf(a->prev_pos.y, a->player.pos.y, alpha) + EYE,
                  lerpf(a->prev_pos.z, a->player.pos.z, alpha));
    view.yaw = lerpf(a->prev_yaw, a->cam.yaw, alpha);
    view.pitch = lerpf(a->prev_pitch, a->cam.pitch, alpha);
    view_render(&a->scene, &a->meshes, &a->water, &view, r, engine_ss(e),
                (float)VIEW_BUDGET_MS, &a->stats);
    (void)snprintf(line, sizeof line, " %5.1f ms %7u rays depth %d%s%s",
                   engine_draw_ms(e), a->stats.rays, a->stats.depth_max,
                   engine_ss(e) == 2 ? " 2x" : "",
                   a->stats.budget_hit ? " BUDGET" : "");
    render_text(r, 1, 0, line, RGB(120, 230, 140));
    (void)snprintf(line, sizeof line,
                   " WASD walk  arrows look  space jump  Tab map  Esc quit"
                   "   [%.100s]",
                   a->label);
    render_text(r, 1, 1, line, RGB(96, 132, 160));
}

int main(int argc, char **argv)
{
    EngineHooks hooks = {app_poll, app_sim, app_draw};
    App a;
    Engine *e;
    int gem;

    memset(&a, 0, sizeof a);
    input_clear(&a.input);
    gem = mesh_load(&a.meshes, "assets/gem.obj");
    a.gem = gem;
    a.cam.fov = 1.05f;
    a.cam.pitch = -0.05f;
    a.player.radius = 0.3f;
    a.player.height = 1.7f;
    a.player.step = 0.42f;
    a.map_count = scan_maps(a.maps);
    a.cur = 0;
    if (argc >= 2) {
        int i;
        int slot = -1;

        for (i = 0; i < a.map_count; i++) {
            if (strcmp(a.maps[i], argv[1]) == 0) {
                slot = i + 1;
                break;
            }
        }
        if (slot < 0 && a.map_count < MAP_MAX) {
            (void)snprintf(a.maps[a.map_count], MAP_NAME, "%s", argv[1]);
            a.map_count++;
            slot = a.map_count;
        }
        load_slot(&a, slot > 0 ? slot : 0);
    } else {
        load_slot(&a, 0);
    }
    e = engine_open(&hooks, &a);
    if (e == NULL) {
        (void)fprintf(stderr, "iota: no terminal to draw in\n");
        return 1;
    }
    engine_run(e);
    engine_close(e);
    return 0;
}


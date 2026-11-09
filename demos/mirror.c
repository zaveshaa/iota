#include "engine.h"
#include "input.h"
#include "scene.h"
#include "view.h"

#include <stdio.h>
#include <string.h>

#define PITCH_MAX 1.48f
#define MOVE_SPEED 3.0f
#define TURN_SPEED 2.0f
#define PITCH_SPEED 1.3f

typedef struct {
    Scene scene;
    Meshes meshes;
    Camera cam;
    ViewStats stats;
    Input input;
} App;

static void add_showcase_details(Scene *s)
{
    Obj *o;
    Light *l;

    s->ambient = RGB(34, 40, 58);
    s->fog = RGB(10, 14, 28);
    s->fog_start = 12.0f;
    s->fog_range = 52.0f;
    s->sky_top = RGB(4, 7, 24);
    s->sky_horizon = RGB(78, 42, 92);
    s->sky_moon_dir = v3(-0.45f, 0.42f, 1.0f);
    s->sky_moon = RGB(255, 224, 178);
    s->sky_moon_size = 0.065f;
    s->have_moon = 1;
    s->objs[0].flags = OBJ_TILE;

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(-4.2f, 2.0f, 6.8f);
    o->half = v3(0.06f, 2.0f, 1.7f);
    o->ink = RGB(142, 194, 214);
    o->flags = OBJ_GLASS;

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(4.2f, 2.0f, 6.8f);
    o->half = v3(0.06f, 2.0f, 1.7f);
    o->ink = RGB(224, 196, 156);
    o->flags = OBJ_SHOJI;

    o = scene_add_obj(s);
    o->kind = OBJ_SPHERE;
    o->pos = v3(-1.2f, 0.9f, 5.2f);
    o->half = v3(0.9f, 0.9f, 0.9f);
    o->ink = RGB(198, 76, 158);

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(-4.0f, 0.65f, 3.2f);
    o->half = v3(0.35f, 0.65f, 0.35f);
    o->ink = RGB(242, 170, 72);

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(4.0f, 0.9f, 3.8f);
    o->half = v3(0.45f, 0.9f, 0.45f);
    o->ink = RGB(106, 224, 190);

    l = scene_add_light(s);
    l->kind = LIGHT_POINT;
    l->pos = v3(3.8f, 2.4f, 5.0f);
    l->color = v3(0.45f, 0.75f, 1.0f);
    l->intensity = 2.2f;

    l = scene_add_light(s);
    l->kind = LIGHT_SUN;
    l->dir = v3_norm(v3(-0.3f, -0.8f, 0.5f));
    l->color = v3(0.52f, 0.60f, 1.0f);
    l->intensity = 0.65f;
}

static float clamp_pitch(float p)
{
    if (p > PITCH_MAX) {
        return PITCH_MAX;
    }
    if (p < -PITCH_MAX) {
        return -PITCH_MAX;
    }
    return p;
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
        default:
            break;
        }
    }
}

static void app_sim(Engine *e, float dt)
{
    App *a = engine_user(e);
    double now = engine_time(e);
    float cp = cosf(a->cam.pitch);
    float step = MOVE_SPEED * dt;
    Vec3 fwd = v3(cp * sinf(a->cam.yaw), sinf(a->cam.pitch),
                  cp * cosf(a->cam.yaw));
    Vec3 right = v3_norm(v3_cross(fwd, v3(0.0f, 1.0f, 0.0f)));

    if (input_held(&a->input, 'w', now)) {
        a->cam.pos = v3_add(a->cam.pos, v3_mul(fwd, step));
    }
    if (input_held(&a->input, 's', now)) {
        a->cam.pos = v3_sub(a->cam.pos, v3_mul(fwd, step));
    }
    if (input_held(&a->input, 'd', now)) {
        a->cam.pos = v3_add(a->cam.pos, v3_mul(right, step));
    }
    if (input_held(&a->input, 'a', now)) {
        a->cam.pos = v3_sub(a->cam.pos, v3_mul(right, step));
    }
    if (input_held(&a->input, KEY_LEFT, now)) {
        a->cam.yaw += TURN_SPEED * dt;
    }
    if (input_held(&a->input, KEY_RIGHT, now)) {
        a->cam.yaw -= TURN_SPEED * dt;
    }
    if (input_held(&a->input, KEY_UP, now)) {
        a->cam.pitch = clamp_pitch(a->cam.pitch + PITCH_SPEED * dt);
    }
    if (input_held(&a->input, KEY_DOWN, now)) {
        a->cam.pitch = clamp_pitch(a->cam.pitch - PITCH_SPEED * dt);
    }
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    Render *r = engine_render(e);
    char line[96];
    char tag[8];

    view_render(&a->scene, &a->meshes, NULL, &a->cam, r, engine_ss(e),
                (float)VIEW_BUDGET_MS, &a->stats);
    tag[0] = '\0';
    if (engine_ss(e) > 1) {
        (void)snprintf(tag, sizeof tag, " %dx", engine_ss(e));
    }
    (void)snprintf(line, sizeof line, " %5.1f ms %7u rays depth %d%s%s",
                   engine_draw_ms(e), a->stats.rays, a->stats.depth_max,
                   tag, a->stats.budget_hit ? " BUDGET" : "");
    render_text(r, 1, 0, line, RGB(120, 230, 140));
    render_text(r, 1, 1, " WASD move  arrows look  Esc quit",
                RGB(96, 132, 160));
}

int main(void)
{
    EngineHooks hooks = {app_poll, app_sim, app_draw};
    App a;
    Engine *e;

    memset(&a, 0, sizeof a);
    input_clear(&a.input);
    mirror_scene(&a.scene, &a.cam);
    add_showcase_details(&a.scene);
    e = engine_open(&hooks, &a);
    if (e == NULL) {
        (void)fprintf(stderr, "iota: no terminal to draw in\n");
        return 1;
    }
    engine_run(e);
    engine_close(e);
    return 0;
}

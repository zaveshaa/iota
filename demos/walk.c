#include "engine.h"
#include "input.h"
#include "view.h"
#include "walk.h"
#include "world.h"

#include <stdio.h>
#include <string.h>

#define EYE 1.6f
#define SPEED 6.0f
#define TURN_SPEED 2.0f
#define PITCH_SPEED 1.3f
#define PITCH_MAX 1.4f

typedef struct {
    Scene scene;
    Meshes meshes;
    Camera cam;
    Walk player;
    ViewStats stats;
    Input input;
    int want_jump;
    Vec3 prev_pos;
    float prev_yaw;
    float prev_pitch;
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
    add_box(s, v3(0.0f, -0.5f, 0.0f), v3(20.0f, 0.5f, 20.0f),
            RGB(96, 100, 116));

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
    a->cam.pos = v3(a->player.pos.x, a->player.pos.y + EYE, a->player.pos.z);
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    Render *r = engine_render(e);
    Camera view = a->cam;
    float alpha = (float)engine_alpha(e);
    char line[96];

    view.pos = v3(lerpf(a->prev_pos.x, a->player.pos.x, alpha),
                  lerpf(a->prev_pos.y, a->player.pos.y, alpha) + EYE,
                  lerpf(a->prev_pos.z, a->player.pos.z, alpha));
    view.yaw = lerpf(a->prev_yaw, a->cam.yaw, alpha);
    view.pitch = lerpf(a->prev_pitch, a->cam.pitch, alpha);
    view_render(&a->scene, &a->meshes, &view, r, engine_ss(e),
                (float)VIEW_BUDGET_MS, &a->stats);
    (void)snprintf(line, sizeof line, " %5.1f ms %7u rays depth %d%s%s",
                   engine_draw_ms(e), a->stats.rays, a->stats.depth_max,
                   engine_ss(e) == 2 ? " 2x" : "",
                   a->stats.budget_hit ? " BUDGET" : "");
    render_text(r, 1, 0, line, RGB(120, 230, 140));
    render_text(r, 1, 1, " WASD walk  arrows look  space jump  Esc quit",
                RGB(96, 132, 160));
}

int main(void)
{
    EngineHooks hooks = {app_poll, app_sim, app_draw};
    App a;
    Engine *e;

    memset(&a, 0, sizeof a);
    input_clear(&a.input);
    build_range(&a.scene, mesh_load(&a.meshes, "assets/gem.obj"));
    a.cam.fov = 1.05f;
    a.cam.pitch = -0.05f;
    a.player.radius = 0.3f;
    a.player.height = 1.7f;
    a.player.step = 0.42f;
    a.player.pos = v3(0.0f, 0.0f, -3.0f);
    a.cam.pos = v3(0.0f, EYE, -3.0f);
    e = engine_open(&hooks, &a);
    if (e == NULL) {
        (void)fprintf(stderr, "iota: no terminal to draw in\n");
        return 1;
    }
    engine_run(e);
    engine_close(e);
    return 0;
}

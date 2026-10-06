#include "engine.h"
#include "scene.h"
#include "view.h"

#include <stdio.h>
#include <string.h>

#define KEY_WINDOW 0.16
#define PITCH_MAX 1.48f
#define MOVE_SPEED 3.0f
#define TURN_SPEED 2.0f
#define PITCH_SPEED 1.3f

enum {
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_TURN_LEFT,
    KEY_TURN_RIGHT,
    KEY_LOOK_UP,
    KEY_LOOK_DOWN,
    KEY_COUNT
};

typedef struct {
    Scene scene;
    Camera cam;
    ViewStats stats;
    double press[KEY_COUNT];
} App;

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

        switch (lc) {
        case KEY_ESC:
        case 3:
            engine_quit(e);
            break;
        case KEY_LEFT:
            a->press[KEY_TURN_LEFT] = now;
            break;
        case KEY_RIGHT:
            a->press[KEY_TURN_RIGHT] = now;
            break;
        case KEY_UP:
            a->press[KEY_LOOK_UP] = now;
            break;
        case KEY_DOWN:
            a->press[KEY_LOOK_DOWN] = now;
            break;
        case 'w':
            a->press[KEY_W] = now;
            break;
        case 'a':
            a->press[KEY_A] = now;
            break;
        case 's':
            a->press[KEY_S] = now;
            break;
        case 'd':
            a->press[KEY_D] = now;
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
    Vec3 dirs[4];
    int i;

    dirs[KEY_W] = fwd;
    dirs[KEY_A] = v3_mul(right, -1.0f);
    dirs[KEY_S] = v3_mul(fwd, -1.0f);
    dirs[KEY_D] = right;

    for (i = 0; i < 4; i++) {
        if (now - a->press[i] < KEY_WINDOW) {
            a->cam.pos = v3_add(a->cam.pos, v3_mul(dirs[i], step));
        }
    }
    if (now - a->press[KEY_TURN_LEFT] < KEY_WINDOW) {
        a->cam.yaw += TURN_SPEED * dt;
    }
    if (now - a->press[KEY_TURN_RIGHT] < KEY_WINDOW) {
        a->cam.yaw -= TURN_SPEED * dt;
    }
    if (now - a->press[KEY_LOOK_UP] < KEY_WINDOW) {
        a->cam.pitch = clamp_pitch(a->cam.pitch + PITCH_SPEED * dt);
    }
    if (now - a->press[KEY_LOOK_DOWN] < KEY_WINDOW) {
        a->cam.pitch = clamp_pitch(a->cam.pitch - PITCH_SPEED * dt);
    }
}

static void app_draw(Engine *e)
{
    App *a = engine_user(e);
    Render *r = engine_render(e);
    char line[96];

    view_render(&a->scene, &a->cam, r, engine_ss(e),
                (float)VIEW_BUDGET_MS, &a->stats);
    (void)snprintf(line, sizeof line, " %5.1f ms %7u rays depth %d%s%s",
                   engine_draw_ms(e), a->stats.rays, a->stats.depth_max,
                   engine_ss(e) == 2 ? " 2x" : "",
                   a->stats.budget_hit ? " BUDGET" : "");
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
    mirror_scene(&a.scene, &a.cam);
    e = engine_open(&hooks, &a);
    if (e == NULL) {
        (void)fprintf(stderr, "iota: no terminal to draw in\n");
        return 1;
    }
    engine_run(e);
    engine_close(e);
    return 0;
}

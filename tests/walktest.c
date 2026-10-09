#include "walk.h"
#include "world.h"

#include <math.h>
#include <stdio.h>

static int g_checks;
static int g_failures;

static void check(int ok, const char *name)
{
    g_checks++;
    if (ok) {
        (void)printf("ok   %s\n", name);
        return;
    }
    g_failures++;
    (void)printf("FAIL %s\n", name);
}

static void add_box(Scene *s, Vec3 pos, Vec3 half)
{
    Obj *o = scene_add_obj(s);

    o->kind = OBJ_BOX;
    o->pos = pos;
    o->half = half;
}

static void floor_scene(Scene *s)
{
    scene_clear(s);
    add_box(s, v3(0.0f, -0.5f, 0.0f), v3(50.0f, 0.5f, 50.0f));
}

static Walk make_walk(Vec3 pos)
{
    Walk w;

    w.pos = pos;
    w.vel = v3(0.0f, 0.0f, 0.0f);
    w.radius = 0.3f;
    w.height = 1.7f;
    w.step = 0.42f;
    w.on_ground = 0;
    w.jump = 0;
    return w;
}

static float flat_speed(Vec3 v)
{
    return sqrtf(v.x * v.x + v.z * v.z);
}

static void run(Walk *w, const Scene *s, Vec3 wish, float dt_total)
{
    float left = dt_total;

    while (left > 0.0f) {
        float dt = left > (1.0f / 60.0f) ? (1.0f / 60.0f) : left;

        walk_move(s, w, wish, 6.0f, dt);
        left -= dt;
    }
}

static void rest_and_speed(const Scene *s)
{
    Walk w = make_walk(v3(0.0f, 0.0f, 0.0f));

    run(&w, s, v3(0.0f, 0.0f, 0.0f), 0.5f);
    check(w.on_ground && fabsf(w.pos.y) < 1e-3f,
          "a body dropped on the floor comes to rest on it");

    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    w.vel = v3(5.0f, 0.0f, 0.0f);
    run(&w, s, v3(0.0f, 0.0f, 0.0f), 1.0f);
    check(flat_speed(w.vel) < 0.01f, "letting go brings a slide to a stop");

    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, s, v3(1.0f, 0.0f, 0.0f), 1.0f);
    check(flat_speed(w.vel) > 5.5f && flat_speed(w.vel) <= 6.001f,
          "holding a direction tops out at the speed that was asked for");
}

static void steps_and_walls(const Scene *s)
{
    Walk w;
    Scene t;

    floor_scene(&t);
    add_box(&t, v3(1.0f, 0.15f, 0.0f), v3(0.5f, 0.15f, 2.0f));
    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, &t, v3(1.0f, 0.0f, 0.0f), 0.2f);
    check(w.pos.y > 0.29f && w.pos.x > 0.5f,
          "a body walks up onto a low step");

    floor_scene(&t);
    add_box(&t, v3(2.0f, 1.0f, 0.0f), v3(0.5f, 1.0f, 2.0f));
    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, &t, v3(1.0f, 0.0f, 0.0f), 1.0f);
    check(w.pos.x < 1.25f, "a body is stopped by a wall it cannot step on");

    {
        Walk a = make_walk(v3(0.0f, 0.0f, 0.0f));
        Walk b = make_walk(v3(0.0f, 0.0f, 0.0f));

        run(&a, s, v3(0.3f, 0.0f, 1.0f), 0.7f);
        run(&b, s, v3(0.3f, 0.0f, 1.0f), 0.7f);
        check(a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z,
              "the same walk comes out the same twice");
    }
}

int main(void)
{
    Scene s;

    floor_scene(&s);
    rest_and_speed(&s);
    steps_and_walls(&s);
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

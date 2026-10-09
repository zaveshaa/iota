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

static void run(Walk *w, const Scene *s, const Meshes *ms, Vec3 wish,
                float dt_total)
{
    float left = dt_total;

    while (left > 0.0f) {
        float dt = left > (1.0f / 60.0f) ? (1.0f / 60.0f) : left;

        walk_move(s, ms, w, wish, 6.0f, dt);
        left -= dt;
    }
}

static void rest_and_speed(const Scene *s)
{
    Walk w = make_walk(v3(0.0f, 0.0f, 0.0f));

    run(&w, s, NULL, v3(0.0f, 0.0f, 0.0f), 0.5f);
    check(w.on_ground && fabsf(w.pos.y) < 1e-3f,
          "a body dropped on the floor comes to rest on it");

    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    w.vel = v3(5.0f, 0.0f, 0.0f);
    run(&w, s, NULL, v3(0.0f, 0.0f, 0.0f), 1.0f);
    check(flat_speed(w.vel) < 0.01f, "letting go brings a slide to a stop");

    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, s, NULL, v3(1.0f, 0.0f, 0.0f), 1.0f);
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
    run(&w, &t, NULL, v3(1.0f, 0.0f, 0.0f), 0.2f);
    check(w.pos.y > 0.29f && w.pos.x > 0.5f,
          "a body walks up onto a low step");

    floor_scene(&t);
    add_box(&t, v3(2.0f, 1.0f, 0.0f), v3(0.5f, 1.0f, 2.0f));
    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, &t, NULL, v3(1.0f, 0.0f, 0.0f), 1.0f);
    check(w.pos.x < 1.25f, "a body is stopped by a wall it cannot step on");

    {
        Walk a = make_walk(v3(0.0f, 0.0f, 0.0f));
        Walk b = make_walk(v3(0.0f, 0.0f, 0.0f));

        run(&a, s, NULL, v3(0.3f, 0.0f, 1.0f), 0.7f);
        run(&b, s, NULL, v3(0.3f, 0.0f, 1.0f), 0.7f);
        check(a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z,
              "the same walk comes out the same twice");
    }
}

static void tri(Mesh *m, Vec3 a, Vec3 b, Vec3 c)
{
    (void)mesh_add(m, a, b, c);
}

static void cube(Mesh *m)
{
    Vec3 v[8];

    v[0] = v3(-0.5f, -0.5f, -0.5f);
    v[1] = v3(0.5f, -0.5f, -0.5f);
    v[2] = v3(0.5f, 0.5f, -0.5f);
    v[3] = v3(-0.5f, 0.5f, -0.5f);
    v[4] = v3(-0.5f, -0.5f, 0.5f);
    v[5] = v3(0.5f, -0.5f, 0.5f);
    v[6] = v3(0.5f, 0.5f, 0.5f);
    v[7] = v3(-0.5f, 0.5f, 0.5f);

    tri(m, v[0], v[1], v[2]);
    tri(m, v[0], v[2], v[3]);
    tri(m, v[4], v[6], v[5]);
    tri(m, v[4], v[7], v[6]);
    tri(m, v[0], v[3], v[7]);
    tri(m, v[0], v[7], v[4]);
    tri(m, v[1], v[5], v[6]);
    tri(m, v[1], v[6], v[2]);
    tri(m, v[0], v[4], v[5]);
    tri(m, v[0], v[5], v[1]);
    tri(m, v[3], v[2], v[6]);
    tri(m, v[3], v[6], v[7]);
}

static void mesh_wall(void)
{
    Meshes ms;
    Scene t;
    Walk w;
    Obj *o;

    mesh_init(&ms.items[0], "cube");
    cube(&ms.items[0]);
    (void)mesh_build(&ms.items[0]);
    ms.count = 1;

    floor_scene(&t);
    o = scene_add_obj(&t);
    o->kind = OBJ_MESH;
    o->pos = v3(4.0f, 0.5f, 0.0f);
    o->mesh = 0;

    w = make_walk(v3(0.0f, 0.0f, 0.0f));
    run(&w, &t, &ms, v3(1.0f, 0.0f, 0.0f), 1.5f);
    check(w.pos.x < 3.6f,
          "a body is stopped by a wall of triangles");

    w = make_walk(v3(4.0f, 3.0f, 0.0f));
    run(&w, &t, &ms, v3(0.0f, 0.0f, 0.0f), 0.6f);
    check(w.on_ground && w.pos.y > 0.99f && w.pos.y < 1.01f,
          "a body lands on a mesh and stands on it");
}

int main(void)
{
    Scene s;

    floor_scene(&s);
    rest_and_speed(&s);
    steps_and_walls(&s);
    mesh_wall();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

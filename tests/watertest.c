#include "water.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

static Water make_pool(void)
{
    Water w;

    memset(&w, 0, sizeof w);
    water_open(&w, v3(-1.5f, 0.0f, 0.9f), v3(1.5f, 0.0f, 3.1f), 1.0f, 0.5f);
    return w;
}

static void step(Water *w, float seconds)
{
    float left = seconds;

    while (left > 0.0f) {
        float dt = left > (1.0f / 60.0f) ? (1.0f / 60.0f) : left;

        water_step(w, dt);
        left -= dt;
    }
}

static int is_finite(float x)
{
    return !isnan(x) && !isinf(x);
}

static void cast_finds_surface(void)
{
    Water w = make_pool();
    Vec3 n;
    float t;
    float a;

    step(&w, 0.3f);
    check(water_cast(&w, v3(0.0f, 3.0f, 2.0f), v3(0.0f, -1.0f, 0.0f), 10.0f,
                     &t, &n, &a) >= 0 &&
              fabsf(t - 2.0f) < 0.3f && n.y > 0.9f,
          "a cast from above a quiet pool finds the surface");
    check(water_cast(&w, v3(4.0f, 3.0f, 2.0f), v3(0.0f, -1.0f, 0.0f), 10.0f,
                     &t, &n, &a) < 0,
          "a cast beside the pool finds no water");
    water_close(&w);
}

static void surface_moves(void)
{
    Water w = make_pool();
    float a;
    float b;

    a = water_surface(&w, 0.0f, 2.0f);
    step(&w, 0.5f);
    b = water_surface(&w, 0.0f, 2.0f);
    check(fabsf(a - b) > 1e-4f, "the procedural waves move the surface");
    water_close(&w);
}

static void body_stirs_the_pool(void)
{
    Water a = make_pool();
    Water b = make_pool();
    int differ = 0;
    int i;
    int j;

    step(&a, 0.2f);
    step(&b, 0.2f);
    (void)water_body(&a, v3(0.0f, 0.0f, 2.0f), 0.3f, v3(6.0f, 0.0f, 0.0f),
                     1.0f / 60.0f);
    step(&a, 1.0f / 60.0f);
    step(&b, 1.0f / 60.0f);
    for (j = 0; j < a.nz && !differ; j++) {
        for (i = 0; i < a.nx; i++) {
            float x = a.x0 + (float)i * a.spacing;
            float z = a.z0 + (float)j * a.spacing;
            float da = water_surface(&a, x, z);
            float db = water_surface(&b, x, z);

            if (fabsf(da - db) > 1e-3f) {
                differ = 1;
            }
        }
    }
    check(differ, "a body moving through stirs ripples across the pool");
    water_close(&a);
    water_close(&b);
}

static void ripples_stay_bounded(void)
{
    Water w = make_pool();
    float max_d = 0.0f;
    int ok = 1;
    int k;

    for (k = 0; k < 200; k++) {
        Vec3 feet = v3(-0.8f + 0.02f * (float)k, 0.0f,
                       2.0f + 0.5f * sinf((float)k));

        (void)water_body(&w, feet, 0.3f, v3(5.0f, 0.0f, -3.0f), 1.0f / 60.0f);
        step(&w, 1.0f / 60.0f);
    }
    for (k = 0; k < w.nx * w.nz; k++) {
        float x = w.x0 + (float)(k % w.nx) * w.spacing;
        float z = w.z0 + (float)(k / w.nx) * w.spacing;
        float d = fabsf(water_surface(&w, x, z) - w.base_h);

        if (d > max_d) {
            max_d = d;
        }
        if (!is_finite(water_surface(&w, x, z)) || d > 1.0f) {
            ok = 0;
        }
    }
    check(ok && max_d > 1e-3f,
          "stirred water stays finite and inside its bounds");
    (void)printf("     max surface swing is %.3f\n", (double)max_d);
    water_close(&w);
}

static void spout_stirs_and_steams(void)
{
    Water w = make_pool();
    Water q = make_pool();
    int differs = 0;
    int i;

    w.heat = 1.0f;
    water_spout(&w, v3(0.0f, 2.0f, 2.0f), 1.0f);
    step(&w, 1.0f);
    step(&q, 1.0f);
    check(w.steam_n > 0, "a hot spout lifts steam off the pool");
    check(q.heat == 0.0f && q.steam_n == 0, "a cold pool steams nowhere");
    for (i = 0; i < w.nx * w.nz; i++) {
        float x = w.x0 + (float)(i % w.nx) * w.spacing;
        float z = w.z0 + (float)(i / w.nx) * w.spacing;

        if (fabsf(water_surface(&w, x, z) - water_surface(&q, x, z)) > 1e-3f) {
            differs = 1;
        }
    }
    check(differs, "a spout keeps the pool moving");
    water_close(&w);
    water_close(&q);
}

static void hot_surface_steams_across(void)
{
    Water w = make_pool();
    int n;
    int i;

    w.heat = 1.0f;
    for (n = 0; n < 12; n++) {
        step(&w, 0.25f);
    }
    check(w.steam_n > 5, "a hot pool steams across its whole surface");
    for (i = 0; i < w.steam_n; i++) {
        const Steam *s = &w.steam[i];

        if (s->span < 40.0f) {
            break;
        }
    }
    check(i == w.steam_n, "the puffs live long enough to rise far");
    water_close(&w);
}

static void deterministic(void)
{
    Water a = make_pool();
    Water b = make_pool();
    int same = 1;
    int k;

    for (k = 0; k < 60; k++) {
        (void)water_body(&a, v3(0.0f, 0.0f, 2.0f), 0.3f,
                         v3(4.0f, 0.0f, 0.0f), 1.0f / 60.0f);
        (void)water_body(&b, v3(0.0f, 0.0f, 2.0f), 0.3f,
                         v3(4.0f, 0.0f, 0.0f), 1.0f / 60.0f);
        step(&a, 1.0f / 60.0f);
        step(&b, 1.0f / 60.0f);
    }
    for (k = 0; k < a.nx * a.nz && same; k++) {
        float x = a.x0 + (float)(k % a.nx) * a.spacing;
        float z = a.z0 + (float)(k / a.nx) * a.spacing;

        if (water_surface(&a, x, z) != water_surface(&b, x, z)) {
            same = 0;
        }
    }
    check(same, "the same pool comes out the same twice");
    water_close(&a);
    water_close(&b);
}

int main(void)
{
    cast_finds_surface();
    surface_moves();
    body_stirs_the_pool();
    ripples_stay_bounded();
    spout_stirs_and_steams();
    hot_surface_steams_across();
    deterministic();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

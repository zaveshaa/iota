#include "sph.h"

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

static Vec3 pool_lo(void)
{
    return v3(-1.5f, 0.0f, 0.9f);
}

static Vec3 pool_hi(void)
{
    return v3(1.5f, 1.3f, 3.1f);
}

static Sph make_pool(void)
{
    Sph sp;

    sph_init(&sp, pool_lo(), pool_hi());
    sph_fill(&sp, v3(-1.5f, 0.0f, 0.9f), v3(1.5f, 0.56f, 3.1f), 0.18f);
    return sp;
}

static void step(Sph *sp, float seconds)
{
    float left = seconds;

    while (left > 0.0f) {
        float dt = left > (1.0f / 60.0f) ? (1.0f / 60.0f) : left;

        sph_step(sp, dt);
        left -= dt;
    }
}

static int is_finite(float x)
{
    return !isnan(x) && !isinf(x);
}

static void in_box(const Sph *sp)
{
    int ok = 1;
    int i;

    for (i = 0; i < sp->count && ok; i++) {
        Vec3 p = sp->pt[i].pos;

        if (!is_finite(p.x) || !is_finite(p.y) || !is_finite(p.z) ||
            p.x < sp->lo_x || p.x > sp->hi_x || p.y < sp->lo_y ||
            p.y > sp->hi_y || p.z < sp->lo_z || p.z > sp->hi_z) {
            ok = 0;
        }
    }
    check(ok, "a settled pool keeps every drop inside");
}

static double avg_dens(const Sph *sp)
{
    double sum = 0.0;
    int i;

    for (i = 0; i < sp->count; i++) {
        sum += sp->pt[i].dens;
    }
    return sum / (double)sp->count;
}

static void rest_density(void)
{
    Sph sp = make_pool();

    step(&sp, 1.2f);
    check(avg_dens(&sp) > SPH_REST * 0.85 &&
              avg_dens(&sp) < SPH_REST * 1.15,
          "a settled pool rests at its natural density");
}

static void body_splash(void)
{
    Sph sp = make_pool();
    Vec3 feet = v3(0.0f, 0.0f, 2.0f);
    Vec3 run = v3(6.0f, 0.0f, 0.0f);
    int splashed = 0;
    int i;

    step(&sp, 0.3f);
    (void)sph_body(&sp, feet, 0.3f, 1.7f, run);
    sph_step(&sp, 1.0f / 60.0f);
    for (i = 0; i < sp.count; i++) {
        if (sp.pt[i].vel.y > 1.5f) {
            splashed = 1;
        }
    }
    check(splashed, "a body wading through throws water upward");
}

static void deterministic(void)
{
    Sph a = make_pool();
    Sph b = make_pool();
    int same = 1;
    int i;

    step(&a, 0.8f);
    step(&b, 0.8f);
    for (i = 0; i < a.count && same; i++) {
        if (a.pt[i].pos.x != b.pt[i].pos.x ||
            a.pt[i].pos.y != b.pt[i].pos.y ||
            a.pt[i].pos.z != b.pt[i].pos.z) {
            same = 0;
        }
    }
    check(same && a.count == b.count,
          "the same pool comes out the same twice");
}

static void stays_in_pool(void)
{
    Sph sp = make_pool();
    Vec3 feet = v3(0.0f, 0.0f, 2.0f);
    float max_y = 0.0f;
    int i;

    step(&sp, 0.2f);
    for (i = 0; i < 10; i++) {
        (void)sph_body(&sp, feet, 0.3f, 1.7f, v3(6.0f, 0.0f, 0.0f));
        feet.z += 0.25f;
        sph_step(&sp, 1.0f / 60.0f);
        if (i == 5 || i == 9) {
            (void)sph_body(&sp, feet, 0.3f, 1.7f, v3(0.0f, 8.0f, 0.0f));
            sph_step(&sp, 1.0f / 60.0f);
        }
    }
    for (i = 0; i < sp.count; i++) {
        if (sp.pt[i].pos.y > max_y) {
            max_y = sp.pt[i].pos.y;
        }
    }
    check(max_y <= sp.hi_y + 1e-3f, "a splash never leaves the pool");
}

int main(void)
{
    Sph sp = make_pool();

    step(&sp, 1.2f);
    in_box(&sp);
    rest_density();
    body_splash();
    deterministic();
    stays_in_pool();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

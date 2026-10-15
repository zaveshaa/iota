#include "water.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define C2 0.40f
#define DAMP 0.010f
#define RIPPLE_MAX 0.6f
#define GRAZE_EPS 0.16f

static float clampf(float x, float lo, float hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

/* 0 in the open water, easing to nothing against the basin walls */
static float shore(const Water *w, float x, float z)
{
    float d1 = x - w->x0;
    float d2 = w->x1 - x;
    float d3 = z - w->z0;
    float d4 = w->z1 - z;
    float dmin = d1 < d2 ? d1 : d2;
    float u;
    float s;

    if (d3 < dmin) {
        dmin = d3;
    }
    if (d4 < dmin) {
        dmin = d4;
    }
    u = clampf((dmin - 0.05f) / 3.5f, 0.0f, 1.0f);
    s = u * u * (3.0f - 2.0f * u);
    return s;
}

static int clamp_i(int x, int lo, int hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

static float wave_sum(float x, float z, float t)
{
    return 0.15f * sinf(0.62f * x - 2.10f * t) +
           0.11f * sinf(0.85f * z + 3.10f * t + 0.90f) +
           0.075f * sinf(0.72f * (x + z) - 4.10f * t + 0.40f);
}

static float ripple_at(const Water *w, float x, float z)
{
    float fx = (x - w->x0) / w->spacing;
    float fz = (z - w->z0) / w->spacing;
    int i0 = clamp_i((int)floorf(fx), 0, w->nx - 1);
    int j0 = clamp_i((int)floorf(fz), 0, w->nz - 1);
    int i1 = clamp_i(i0 + 1, 0, w->nx - 1);
    int j1 = clamp_i(j0 + 1, 0, w->nz - 1);
    float sx = clampf(fx - (float)i0, 0.0f, 1.0f);
    float sz = clampf(fz - (float)j0, 0.0f, 1.0f);
    float a = w->cur[(size_t)j0 * (size_t)w->nx + (size_t)i0];
    float b = w->cur[(size_t)j0 * (size_t)w->nx + (size_t)i1];
    float c = w->cur[(size_t)j1 * (size_t)w->nx + (size_t)i0];
    float d = w->cur[(size_t)j1 * (size_t)w->nx + (size_t)i1];

    return (a * (1.0f - sx) + b * sx) * (1.0f - sz) +
           (c * (1.0f - sx) + d * sx) * sz;
}

float water_surface(const Water *w, float x, float z)
{
    float r = ripple_at(w, x, z);

    if (r < -0.30f) {
        r = -0.30f;
    }
    return w->base_h + shore(w, x, z) * (wave_sum(x, z, w->t) + r);
}

static void ripple_swap(Water *w)
{
    float *tmp = w->prev;

    w->prev = w->cur;
    w->cur = w->next;
    w->next = tmp;
}

static void ripple_step(Water *w)
{
    int i;
    int j;

    for (i = 0; i < w->nx * w->nz; i++) {
        w->next[i] = 0.0f;
    }
    for (j = 1; j < w->nz - 1; j++) {
        for (i = 1; i < w->nx - 1; i++) {
            size_t k = (size_t)j * (size_t)w->nx + (size_t)i;
            float lap =
                w->cur[k - 1] + w->cur[k + 1] +
                w->cur[(size_t)(j - 1) * (size_t)w->nx + (size_t)i] +
                w->cur[(size_t)(j + 1) * (size_t)w->nx + (size_t)i] -
                4.0f * w->cur[k];
            float vel = w->cur[k] - w->prev[k];
            float nxt = w->cur[k] + vel * (1.0f - DAMP) + C2 * lap;

            w->next[k] = clampf(nxt, -RIPPLE_MAX, RIPPLE_MAX);
        }
    }
    ripple_swap(w);
}

void water_open(Water *w, Vec3 lo, Vec3 hi, float base_h, float spacing)
{
    int n;

    if (w->cur != NULL) {
        free(w->cur);
        free(w->prev);
        free(w->next);
    }
    memset(w, 0, sizeof *w);
    w->x0 = lo.x;
    w->z0 = lo.z;
    w->x1 = hi.x;
    w->z1 = hi.z;
    w->base_h = base_h;
    w->spacing = spacing > 0.01f ? spacing : 0.01f;
    w->nx = (int)((hi.x - lo.x) / w->spacing) + 1;
    w->nz = (int)((hi.z - lo.z) / w->spacing) + 1;
    if (w->nx < 2) {
        w->nx = 2;
    }
    if (w->nz < 2) {
        w->nz = 2;
    }
    n = w->nx * w->nz;
    w->cur = calloc((size_t)n, sizeof *w->cur);
    w->prev = calloc((size_t)n, sizeof *w->prev);
    w->next = calloc((size_t)n, sizeof *w->next);
}

void water_close(Water *w)
{
    free(w->cur);
    free(w->prev);
    free(w->next);
    memset(w, 0, sizeof *w);
}

static void add_ripple(Water *w, float x, float z, float radius, float amp);

static float rng_unit(unsigned *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (float)((*state >> 8) & 0xffffu) / 65535.0f;
}

static void steam_spawn(Water *w)
{
    Steam *s;

    if (w->steam_n >= WATER_STEAM_MAX) {
        return;
    }
    s = &w->steam[w->steam_n];
    w->steam_n++;
    s->p = v3(w->spout_x + (rng_unit(&w->rng) - 0.5f) * 0.5f,
              water_surface(w, w->spout_x, w->spout_z) + 0.05f,
              w->spout_z + (rng_unit(&w->rng) - 0.5f) * 0.5f);
    s->v = v3((rng_unit(&w->rng) - 0.5f) * 0.4f,
              0.6f + rng_unit(&w->rng) * 0.9f,
              (rng_unit(&w->rng) - 0.5f) * 0.4f);
    s->span = 0.9f + rng_unit(&w->rng) * 0.9f;
    s->life = s->span;
}

static void steam_step(Water *w, float dt)
{
    int i = 0;

    while (i < w->steam_n) {
        Steam *s = &w->steam[i];

        s->life -= dt;
        if (s->life <= 0.0f) {
            w->steam[i] = w->steam[w->steam_n - 1];
            w->steam_n--;
            continue;
        }
        s->p = v3_add(s->p, v3_mul(s->v, dt));
        s->v.y -= 0.35f * dt;
        s->v.x *= 1.0f - 0.6f * dt;
        s->v.z *= 1.0f - 0.6f * dt;
        i++;
    }
}

void water_spout(Water *w, Vec3 top, float rate)
{
    w->spout_on = 1;
    w->spout_x = top.x;
    w->spout_y = top.y;
    w->spout_z = top.z;
    w->spout_rate = rate > 0.0f ? rate : 0.0f;
}

void water_step(Water *w, float dt)
{
    if (w->cur == NULL) {
        return;
    }
    w->t += dt;
    if (w->spout_on) {
        float pulse = 0.7f + 0.3f * sinf(w->t * 6.5f);

        add_ripple(w, w->spout_x, w->spout_z, 0.32f,
                   (0.9f + 0.3f * pulse) * w->spout_rate * dt);
        w->spawn_acc += w->spout_rate * dt * 14.0f;
        while (w->spawn_acc >= 1.0f) {
            w->spawn_acc -= 1.0f;
            steam_spawn(w);
        }
    }
    steam_step(w, dt);
    ripple_step(w);
}

static void add_ripple(Water *w, float x, float z, float radius, float amp)
{
    float sigma = radius * 0.35f;
    int i;
    int j;
    int i0 = clamp_i((int)((x - radius - w->x0) / w->spacing), 0, w->nx - 1);
    int i1 = clamp_i((int)((x + radius - w->x0) / w->spacing), 0, w->nx - 1);
    int j0 = clamp_i((int)((z - radius - w->z0) / w->spacing), 0, w->nz - 1);
    int j1 = clamp_i((int)((z + radius - w->z0) / w->spacing), 0, w->nz - 1);

    for (j = j0; j <= j1; j++) {
        for (i = i0; i <= i1; i++) {
            float gx = w->x0 + (float)i * w->spacing;
            float gz = w->z0 + (float)j * w->spacing;
            float dx = gx - x;
            float dz = gz - z;
            float g = expf(-(dx * dx + dz * dz) / (2.0f * sigma * sigma));
            size_t k = (size_t)j * (size_t)w->nx + (size_t)i;

            w->cur[k] = clampf(w->cur[k] + amp * g, -RIPPLE_MAX, RIPPLE_MAX);
        }
    }
}

int water_body(Water *w, Vec3 feet, float radius, Vec3 vel, float dt)
{
    float surf;

    if (feet.y >= w->base_h - 1e-3f) {
        return 0;
    }
    surf = water_surface(w, feet.x, feet.z);
    if (feet.y >= surf - 1e-3f) {
        return 0;
    }
    add_ripple(w, feet.x, feet.z, radius + 0.55f,
               (2.20f + 6.50f * v3_len(vel)) * dt);
    return 1;
}

static int in_bounds(const Water *w, float x, float z)
{
    return x >= w->x0 && x <= w->x1 && z >= w->z0 && z <= w->z1;
}

int water_cast(const Water *w, Vec3 origin, Vec3 dir, float tmax,
               float *t_out, Vec3 *n_out, float *a_out)
{
    float base;
    float t0;
    float x;
    float z;
    int i;

    if (w->cur == NULL || w->nx < 2 || w->nz < 2) {
        return -1;
    }
    base = w->base_h;
    if (fabsf(dir.y) < 1e-4f) {
        if (origin.y > base + GRAZE_EPS || origin.y < base - GRAZE_EPS) {
            return -1;
        }
        t0 = tmax;
        x = origin.x + dir.x * t0;
        z = origin.z + dir.z * t0;
    } else {
        t0 = (base - origin.y) / dir.y;
        if (t0 <= 1e-4f) {
            return -1;
        }
        if (t0 > tmax) {
            return -1;
        }
        x = origin.x + dir.x * t0;
        z = origin.z + dir.z * t0;
    }
    if (!in_bounds(w, x, z)) {
        return -1;
    }
    for (i = 0; i < 2; i++) {
        float h = water_surface(w, x, z);

        if (fabsf(dir.y) > 1e-4f) {
            float dt1 = (h - (origin.y + dir.y * t0)) / dir.y;

            t0 += dt1;
            if (t0 <= 1e-4f || t0 > tmax) {
                return -1;
            }
        }
        x = origin.x + dir.x * t0;
        z = origin.z + dir.z * t0;
        if (!in_bounds(w, x, z)) {
            return -1;
        }
    }
    *t_out = t0;
    if (n_out != NULL) {
        float g = w->spacing;
        Vec3 n = v3_norm(v3(-(water_surface(w, x + g, z) -
                              water_surface(w, x - g, z)) /
                                (2.0f * g),
                            1.0f,
                            -(water_surface(w, x, z + g) -
                              water_surface(w, x, z - g)) /
                                (2.0f * g)));

        if (v3_dot(n, dir) > 0.0f) {
            n = v3_mul(n, -1.0f);
        }
        *n_out = n;
    }
    if (a_out != NULL) {
        *a_out = clampf(fabsf(water_surface(w, x, z) - base) * 2.5f, 0.0f,
                        1.0f);
    }
    return 1;
}

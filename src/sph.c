#include "sph.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define H2 (SPH_SMOOTH * SPH_SMOOTH)

static float kpoly6(void)
{
    float h3 = SPH_SMOOTH * SPH_SMOOTH * SPH_SMOOTH;

    return 315.0f / (64.0f * (float)M_PI * h3 * h3 * h3);
}

static float kspiky(void)
{
    float h3 = SPH_SMOOTH * SPH_SMOOTH * SPH_SMOOTH;

    return 45.0f / ((float)M_PI * h3 * h3);
}

static float kvisc(void)
{
    float h3 = SPH_SMOOTH * SPH_SMOOTH * SPH_SMOOTH;

    return 45.0f / ((float)M_PI * h3 * h3);
}

static unsigned hash32(unsigned x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static int clamp_cell(float v, float lo, int g)
{
    int c = (int)((v - lo) / SPH_SMOOTH);

    if (c < 0) {
        c = 0;
    }
    if (c >= g) {
        c = g - 1;
    }
    return c;
}

static int cell_of(const Sph *sp, Vec3 p)
{
    int cx = clamp_cell(p.x, sp->lo_x, sp->gx);
    int cy = clamp_cell(p.y, sp->lo_y, sp->gy);
    int cz = clamp_cell(p.z, sp->lo_z, sp->gz);

    return (cy * sp->gz + cz) * sp->gx + cx;
}

static void grid_build(Sph *sp)
{
    int c;
    int i;

    for (c = 0; c < sp->gx * sp->gy * sp->gz; c++) {
        sp->head[c] = -1;
    }
    for (i = 0; i < sp->count; i++) {
        int cell = cell_of(sp, sp->pt[i].pos);

        sp->next[i] = sp->head[cell];
        sp->head[cell] = i;
    }
}

static void sph_densities(Sph *sp)
{
    int i;

    for (i = 0; i < sp->count; i++) {
        float sum = sp->kpoly * H2 * H2 * H2;
        int cx = clamp_cell(sp->pt[i].pos.x, sp->lo_x, sp->gx);
        int cy = clamp_cell(sp->pt[i].pos.y, sp->lo_y, sp->gy);
        int cz = clamp_cell(sp->pt[i].pos.z, sp->lo_z, sp->gz);
        int dx;
        int dy;
        int dz;

        for (dy = -1; dy <= 1; dy++) {
            for (dz = -1; dz <= 1; dz++) {
                for (dx = -1; dx <= 1; dx++) {
                    int cyy = cy + dy;
                    int czz = cz + dz;
                    int cxx = cx + dx;
                    int j;

                    if (cyy < 0 || cyy >= sp->gy) {
                        continue;
                    }
                    if (czz < 0 || czz >= sp->gz) {
                        continue;
                    }
                    if (cxx < 0 || cxx >= sp->gx) {
                        continue;
                    }
                    j = sp->head[(cyy * sp->gz + czz) * sp->gx + cxx];
                    while (j >= 0) {
                        Vec3 r = v3_sub(sp->pt[i].pos, sp->pt[j].pos);
float d2 = v3_dot(r, r);

                            if (j != i && d2 < H2) {
                                float q = H2 - d2;

                                sum += sp->kpoly * q * q * q;
                            }
                        j = sp->next[j];
                    }
                }
            }
        }
        sp->pt[i].dens = sum;
    }
}

static void sph_forces(Sph *sp)
{
    int i;

    for (i = 0; i < sp->count; i++) {
        Vec3 acc = v3(0.0f, -SPH_G, 0.0f);
        float ri = sp->pt[i].dens;
        int cx = clamp_cell(sp->pt[i].pos.x, sp->lo_x, sp->gx);
        int cy = clamp_cell(sp->pt[i].pos.y, sp->lo_y, sp->gy);
        int cz = clamp_cell(sp->pt[i].pos.z, sp->lo_z, sp->gz);
        int dx;
        int dy;
        int dz;

        for (dy = -1; dy <= 1; dy++) {
            for (dz = -1; dz <= 1; dz++) {
                for (dx = -1; dx <= 1; dx++) {
                    int cyy = cy + dy;
                    int czz = cz + dz;
                    int cxx = cx + dx;
                    int j;

                    if (cyy < 0 || cyy >= sp->gy) {
                        continue;
                    }
                    if (czz < 0 || czz >= sp->gz) {
                        continue;
                    }
                    if (cxx < 0 || cxx >= sp->gx) {
                        continue;
                    }
                    j = sp->head[(cyy * sp->gz + czz) * sp->gx + cxx];
                    while (j >= 0) {
                        if (j != i) {
                            Vec3 r = v3_sub(sp->pt[i].pos, sp->pt[j].pos);
                            float d2 = v3_dot(r, r);

                            if (d2 < H2 && d2 > 1e-9f) {
                                float d = sqrtf(d2);
                                Vec3 dir = v3_mul(r, 1.0f / d);
                                float rj = sp->pt[j].dens;
                                float p_i = SPH_PRESS *
                                            (ri - SPH_REST);
                                float p_j = SPH_PRESS *
                                            (rj - SPH_REST);

                                if (p_i < 0.0f) {
                                    p_i = 0.0f;
                                }
                                if (p_j < 0.0f) {
                                    p_j = 0.0f;
                                }
                                acc = v3_add(
                                    acc, v3_mul(
                                             dir, sp->kspiky * (H2 - d2) *
                                                      (p_i / (ri * ri) +
                                                       p_j / (rj * rj))));
                                acc = v3_add(
                                    acc, v3_mul(
                                             v3_sub(sp->pt[j].vel,
                                                    sp->pt[i].vel),
                                             SPH_VISC * sp->kvisc *
                                                 (SPH_SMOOTH - d) / rj));
                            }
                        }
                        j = sp->next[j];
                    }
                }
            }
        }
        sp->pt[i].vel = v3_add(sp->pt[i].vel, v3_mul(acc, SPH_DT));
    }
}

static void sph_integrate(Sph *sp)
{
    int i;

    for (i = 0; i < sp->count; i++) {
        Vec3 p = v3_add(sp->pt[i].pos, v3_mul(sp->pt[i].vel, SPH_DT));
        Vec3 v = sp->pt[i].vel;

        if (p.x < sp->lo_x) {
            p.x = sp->lo_x;
            if (v.x < 0.0f) {
                v.x = -v.x * 0.35f;
            }
        } else if (p.x > sp->hi_x) {
            p.x = sp->hi_x;
            if (v.x > 0.0f) {
                v.x = -v.x * 0.35f;
            }
        }
        if (p.y < sp->lo_y) {
            p.y = sp->lo_y;
            if (v.y < 0.0f) {
                v.y = -v.y * 0.35f;
            }
        } else if (p.y > sp->hi_y) {
            p.y = sp->hi_y;
            if (v.y > 0.0f) {
                v.y = -v.y * 0.35f;
            }
        }
        if (p.z < sp->lo_z) {
            p.z = sp->lo_z;
            if (v.z < 0.0f) {
                v.z = -v.z * 0.35f;
            }
        } else if (p.z > sp->hi_z) {
            p.z = sp->hi_z;
            if (v.z > 0.0f) {
                v.z = -v.z * 0.35f;
            }
        }
        sp->pt[i].pos = p;
        sp->pt[i].vel = v;
    }
}

void sph_init(Sph *sp, Vec3 lo, Vec3 hi)
{
    memset(sp, 0, sizeof *sp);
    sp->lo_x = lo.x;
    sp->lo_y = lo.y;
    sp->lo_z = lo.z;
    sp->hi_x = hi.x;
    sp->hi_y = hi.y;
    sp->hi_z = hi.z;
    sp->gx = (int)((hi.x - lo.x) / SPH_SMOOTH) + 2;
    sp->gy = (int)((hi.y - lo.y) / SPH_SMOOTH) + 2;
    sp->gz = (int)((hi.z - lo.z) / SPH_SMOOTH) + 2;
    sp->kpoly = kpoly6();
    sp->kspiky = kspiky();
    sp->kvisc = kvisc();
}

void sph_fill(Sph *sp, Vec3 lo, Vec3 hi, float spacing)
{
    float x;
    float y;
    float z;

    for (y = lo.y; y <= hi.y + 1e-6f * spacing; y += spacing) {
        for (z = lo.z; z <= hi.z + 1e-6f * spacing; z += spacing) {
            for (x = lo.x; x <= hi.x + 1e-6f * spacing; x += spacing) {
                Vec3 p = v3(x, y, z);
                unsigned h = hash32((unsigned)sp->count * 2654435761u + 1u);

                p.x += ((float)((h >> 8) & 255u) / 255.0f - 0.5f) * spacing *
                       0.4f;
                p.y += ((float)((h >> 16) & 255u) / 255.0f - 0.5f) * spacing *
                       0.4f;
                p.z += ((float)((h >> 24) & 255u) / 255.0f - 0.5f) * spacing *
                       0.4f;
                if (sp->count < SPH_MAX) {
                    SphPt *s = &sp->pt[sp->count];

                    s->pos = p;
                    s->vel = v3(0.0f, 0.0f, 0.0f);
                    s->dens = SPH_REST;
                    sp->count++;
                }
            }
        }
    }
}

void sph_step(Sph *sp, float dt)
{
    float left = dt;

    while (left > 0.0f) {
        float h = left > SPH_DT ? SPH_DT : left;

        grid_build(sp);
        sph_densities(sp);
        sph_forces(sp);
        sph_integrate(sp);
        left -= h;
    }
    grid_build(sp);
}

static Vec3 body_dir(int i)
{
    unsigned h = hash32((unsigned)i * 40503u + 7u);
    float a = (float)(h & 0xffffu) / 65535.0f * 2.0f * (float)M_PI;

    return v3(cosf(a), 0.0f, sinf(a));
}

int sph_body(Sph *sp, Vec3 feet, float radius, float height, Vec3 vel)
{
    float rr = radius + SPH_PT_R + 0.02f;
    int n = 0;
    int i;

    for (i = 0; i < sp->count; i++) {
        Vec3 d = v3_sub(sp->pt[i].pos, feet);
        float hy = d.y;
        float hd;
        Vec3 dir;

        d.y = 0.0f;
        hd = v3_len(d);
        if (hd >= rr || hy < -SPH_PT_R || hy > height + SPH_PT_R) {
            continue;
        }
        dir = hd > 1e-4f ? v3_mul(d, 1.0f / hd) : body_dir(i);
        {
            float push = rr - hd;
            float hs = sqrtf(vel.x * vel.x + vel.z * vel.z);

            sp->pt[i].pos.x += dir.x * push * 0.5f;
            sp->pt[i].pos.z += dir.z * push * 0.5f;
            if (hy < 0.0f) {
                sp->pt[i].pos.y -= push * 0.5f;
            }
            sp->pt[i].vel.x = 0.45f * sp->pt[i].vel.x + 0.35f * vel.x +
                              dir.x * 2.0f;
            sp->pt[i].vel.z = 0.45f * sp->pt[i].vel.z + 0.35f * vel.z +
                              dir.z * 2.0f;
            sp->pt[i].vel.y += (hy < 0.0f ? 1.0f : 0.0f) * (0.5f + hs) *
                               (SPH_PT_R + 0.02f) / SPH_PT_R;
        }
        n++;
    }
    return n;
}

static int ray_aabb(Vec3 o, Vec3 d, const Sph *sp, float tmax, float *t_out)
{
    float tmin = -FLT_MAX;
    float tmaxe = FLT_MAX;
    float lo[3] = {sp->lo_x, sp->lo_y, sp->lo_z};
    float hi[3] = {sp->hi_x, sp->hi_y, sp->hi_z};
    float oa[3] = {o.x, o.y, o.z};
    float da[3] = {d.x, d.y, d.z};
    int axis;

    for (axis = 0; axis < 3; axis++) {
        if (da[axis] > -1e-9f && da[axis] < 1e-9f) {
            if (oa[axis] < lo[axis] || oa[axis] > hi[axis]) {
                return 0;
            }
            continue;
        }
        {
            float t1 = (lo[axis] - oa[axis]) / da[axis];
            float t2 = (hi[axis] - oa[axis]) / da[axis];
            float tmp;

            if (t1 > t2) {
                tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tmin) {
                tmin = t1;
            }
            if (t2 < tmaxe) {
                tmaxe = t2;
            }
            if (tmin > tmaxe) {
                return 0;
            }
        }
    }
    if (tmin < 0.0f) {
        tmin = 0.0f;
    }
    if (tmin > tmax) {
        return 0;
    }
    *t_out = tmin;
    return 1;
}

int sph_cast(const Sph *sp, Vec3 origin, Vec3 dir, float tmax, float *t_out,
             Vec3 *n_out)
{
    Vec3 start;
    float tenter;
    float tbound = 0.0f;
    int cx;
    int cy;
    int cz;
    int sx;
    int sy;
    int sz;
    float tmax_x;
    float tmax_y;
    float tmax_z;
    float tdx;
    float tdy;
    float tdz;
    int best = -1;
    float best_t = tmax;
    int axis;

    if (sp->count == 0) {
        return -1;
    }
    if (!ray_aabb(origin, dir, sp, tmax, &tenter)) {
        return -1;
    }
    start = v3_add(origin, v3_mul(dir, tenter));
    cx = clamp_cell(start.x, sp->lo_x, sp->gx);
    cy = clamp_cell(start.y, sp->lo_y, sp->gy);
    cz = clamp_cell(start.z, sp->lo_z, sp->gz);
    sx = dir.x > 0.0f ? 1 : -1;
    sy = dir.y > 0.0f ? 1 : -1;
    sz = dir.z > 0.0f ? 1 : -1;

    if (dir.x > -1e-9f && dir.x < 1e-9f) {
        tmax_x = FLT_MAX;
        tdx = FLT_MAX;
    } else {
        tdx = SPH_SMOOTH / fabsf(dir.x);
        tmax_x = (sx > 0 ? (sp->lo_x + (float)(cx + 1) * SPH_SMOOTH) - start.x
                         : (sp->lo_x + (float)cx * SPH_SMOOTH) - start.x) /
                 dir.x;
    }
    if (dir.y > -1e-9f && dir.y < 1e-9f) {
        tmax_y = FLT_MAX;
        tdy = FLT_MAX;
    } else {
        tdy = SPH_SMOOTH / fabsf(dir.y);
        tmax_y = (sy > 0 ? (sp->lo_y + (float)(cy + 1) * SPH_SMOOTH) - start.y
                         : (sp->lo_y + (float)cy * SPH_SMOOTH) - start.y) /
                 dir.y;
    }
    if (dir.z > -1e-9f && dir.z < 1e-9f) {
        tmax_z = FLT_MAX;
        tdz = FLT_MAX;
    } else {
        tdz = SPH_SMOOTH / fabsf(dir.z);
        tmax_z = (sz > 0 ? (sp->lo_z + (float)(cz + 1) * SPH_SMOOTH) - start.z
                         : (sp->lo_z + (float)cz * SPH_SMOOTH) - start.z) /
                 dir.z;
    }

    for (;;) {
        int cell = (cy * sp->gz + cz) * sp->gx + cx;
        int j = sp->head[cell];

        tbound = tmax_x;
        axis = 0;
        if (tmax_y < tbound) {
            tbound = tmax_y;
            axis = 1;
        }
        if (tmax_z < tbound) {
            tbound = tmax_z;
            axis = 2;
        }
        while (j >= 0) {
            Vec3 rel = v3_sub(sp->pt[j].pos, origin);
            float b = v3_dot(rel, dir);
            float c = v3_dot(rel, rel) - SPH_PT_R * SPH_PT_R;
            float disc = b * b - c;

            if (disc > 0.0f) {
                float t = -b - sqrtf(disc);

                if (t < 0.0f) {
                    t = -b + sqrtf(disc);
                }
                if (t >= 0.0f && t < best_t) {
                    best = j;
                    best_t = t;
                    *t_out = t;
                    *n_out = v3_mul(
                        v3_sub(v3_add(origin, v3_mul(dir, t)),
                               sp->pt[j].pos),
                        1.0f / SPH_PT_R);
                }
            }
            j = sp->next[j];
        }
        if (best >= 0 && best_t < tbound) {
            break;
        }
        if (tbound >= tmax) {
            break;
        }
        if (axis == 0) {
            cx += sx;
            tmax_x += tdx;
        } else if (axis == 1) {
            cy += sy;
            tmax_y += tdy;
        } else {
            cz += sz;
            tmax_z += tdz;
        }
        if (cx < 0 || cx >= sp->gx || cy < 0 || cy >= sp->gy || cz < 0 ||
            cz >= sp->gz) {
            break;
        }
    }
    return best;
}

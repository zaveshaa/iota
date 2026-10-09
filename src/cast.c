#include "cast.h"

#include <float.h>
#include <stddef.h>

#define HIT_EPS 1e-4f

static int ray_sphere(Vec3 o, Vec3 d, Vec3 c, float r, float *t_out)
{
    Vec3 oc = v3_sub(o, c);
    float a = v3_dot(d, d);
    float b = v3_dot(oc, d);
    float cc = v3_dot(oc, oc) - r * r;
    float disc = b * b - a * cc;
    float t;

    if (disc < 0.0f || a <= 0.0f) {
        return 0;
    }
    t = (-b - sqrtf(disc)) / a;
    if (t <= HIT_EPS) {
        t = (-b + sqrtf(disc)) / a;
    }
    if (t <= HIT_EPS) {
        return 0;
    }
    *t_out = t;
    return 1;
}

static int ray_box(Vec3 o, Vec3 d, Vec3 c, Vec3 half, float *t_out,
                   Vec3 *n_out)
{
    float tmin = -FLT_MAX;
    float tmax = FLT_MAX;
    int entry_axis = 0;
    float entry_sign = 1.0f;
    int axis;

    for (axis = 0; axis < 3; axis++) {
        float oi = axis == 0 ? o.x : (axis == 1 ? o.y : o.z);
        float di = axis == 0 ? d.x : (axis == 1 ? d.y : d.z);
        float ci = axis == 0 ? c.x : (axis == 1 ? c.y : c.z);
        float hi = axis == 0 ? half.x : (axis == 1 ? half.y : half.z);
        float lo = ci - hi;
        float up = ci + hi;
        float t1;
        float t2;
        float swap;

        if (di > -1e-9f && di < 1e-9f) {
            if (oi < lo || oi > up) {
                return 0;
            }
            continue;
        }
        t1 = (lo - oi) / di;
        t2 = (up - oi) / di;
        if (t1 > t2) {
            swap = t1;
            t1 = t2;
            t2 = swap;
        }
        if (t1 > tmin) {
            tmin = t1;
            entry_axis = axis;
            entry_sign = di > 0.0f ? -1.0f : 1.0f;
        }
        if (t2 < tmax) {
            tmax = t2;
        }
        if (tmin > tmax) {
            return 0;
        }
    }
    if (tmin <= HIT_EPS) {
        return 0;
    }
    *t_out = tmin;
    *n_out = v3(entry_sign * (entry_axis == 0 ? 1.0f : 0.0f),
                entry_sign * (entry_axis == 1 ? 1.0f : 0.0f),
                entry_sign * (entry_axis == 2 ? 1.0f : 0.0f));
    return 1;
}

static int ray_plane(Vec3 o, Vec3 d, Vec3 p, Vec3 n, float *t_out)
{
    float den = v3_dot(d, n);
    float t;

    if (den > -1e-9f && den < 1e-9f) {
        return 0;
    }
    t = v3_dot(v3_sub(p, o), n) / den;
    if (t <= HIT_EPS) {
        return 0;
    }
    *t_out = t;
    return 1;
}

Hit cast_ray(const Scene *s, const Meshes *ms, Vec3 origin, Vec3 dir,
             float tmax)
{
    Hit best;
    int i;

    best.action = HIT_NONE;
    best.t = tmax;
    best.point = v3(0.0f, 0.0f, 0.0f);
    best.normal = v3(0.0f, 0.0f, 0.0f);
    best.obj = NULL;

    for (i = 0; i < s->obj_count; i++) {
        const Obj *o = &s->objs[i];
        float t = 0.0f;
        Vec3 n = v3(0.0f, 0.0f, 0.0f);
        int hit = 0;

        if (o->kind == OBJ_SPHERE) {
            float radius = o->half.x;

            hit = ray_sphere(origin, dir, o->pos, radius, &t);
            if (hit) {
                n = v3_mul(v3_sub(v3_add(origin, v3_mul(dir, t)), o->pos),
                           1.0f / radius);
            }
        } else if (o->kind == OBJ_BOX) {
            hit = ray_box(origin, dir, o->pos, o->half, &t, &n);
        } else if (o->kind == OBJ_MESH) {
            // the triangles live at the origin, so the ray is pulled back to the
            // object's own frame and the hit distance is the world's already
            if (ms != NULL && o->mesh >= 0 && o->mesh < ms->count) {
                Vec3 local = v3_sub(origin, o->pos);

                hit = mesh_tri_hit(&ms->items[o->mesh], local, dir, &t, &n);
            }
        } else {
            hit = ray_plane(origin, dir, o->pos, o->axis, &t);
            if (hit) {
                n = o->axis;
            }
        }
        if (!hit || t >= best.t) {
            continue;
        }
        if (v3_dot(n, dir) > 0.0f) {
            n = v3_mul(n, -1.0f);
        }
        best.action = (o->flags & OBJ_MIRROR) != 0u ? HIT_MIRROR
                                                    : HIT_CONTINUE;
        best.t = t;
        best.point = v3_add(origin, v3_mul(dir, t));
        best.normal = n;
        best.obj = o;
    }
    return best;
}

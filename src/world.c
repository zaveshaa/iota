#include "world.h"

#include <stdio.h>
#include <string.h>

void world_init(World *w) {
    w->count = 0;
}

int world_add(World *w, Obj o) {
    if (w->count >= WORLD_MAX) return -1;
    w->items[w->count] = o;
    return w->count++;
}

void world_remove(World *w, int idx) {
    if (idx < 0 || idx >= w->count) return;
    for (int i = idx; i < w->count - 1; i++) w->items[i] = w->items[i + 1];
    w->count--;
}

// slab method
static int hit_box(const Obj *o, Vec3 ro, Vec3 rd, float *tout, Vec3 *nout) {
    Vec3 mn = v3_sub(o->pos, o->half);
    Vec3 mx = v3_add(o->pos, o->half);

    float roa[3] = {ro.x, ro.y, ro.z};
    float rda[3] = {rd.x, rd.y, rd.z};
    float mna[3] = {mn.x, mn.y, mn.z};
    float mxa[3] = {mx.x, mx.y, mx.z};

    float tmin = -1e30f, tmax = 1e30f;
    int axis = 0;

    for (int i = 0; i < 3; i++) {
        if (__builtin_fabsf(rda[i]) < 1e-8f) {
            if (roa[i] < mna[i] || roa[i] > mxa[i]) return 0;
            continue;
        }
        float inv = 1.0f / rda[i];
        float t1 = (mna[i] - roa[i]) * inv;
        float t2 = (mxa[i] - roa[i]) * inv;
        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tmin) { tmin = t1; axis = i; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return 0;
    }

    if (tmax < 0.0f) return 0;

    int inside = tmin < 0.0f;
    float t = inside ? tmax : tmin;

    Vec3 n = v3(0.0f, 0.0f, 0.0f);
    float sign = (rda[axis] > 0.0f) ? -1.0f : 1.0f;
    if (inside) sign = -sign;
    if (axis == 0) n.x = sign;
    else if (axis == 1) n.y = sign;
    else n.z = sign;

    *tout = t;
    *nout = n;
    return 1;
}

// quadratic, first non-negative root
static int hit_sphere(const Obj *o, Vec3 ro, Vec3 rd, float *tout, Vec3 *nout) {
    float r = o->half.x;
    Vec3 oc = v3_sub(ro, o->pos);
    float b = v3_dot(oc, rd);
    float c = v3_dot(oc, oc) - r * r;
    float disc = b * b - c;
    if (disc < 0.0f) return 0;

    float sq = __builtin_sqrtf(disc);
    float t = -b - sq;
    if (t < 0.0f) t = -b + sq;
    if (t < 0.0f) return 0;

    *tout = t;
    *nout = v3_scale(v3_sub(v3_add(ro, v3_scale(rd, t)), o->pos), 1.0f / r);
    return 1;
}

int world_cast(const World *w, Vec3 origin, Vec3 dir, float max_t, Hit *hit) {
    float best = max_t;
    int found = 0;

    // nearest object along the ray wins
    for (int i = 0; i < w->count; i++) {
        float t;
        Vec3 n;
        int ok;
        if (w->items[i].type == OBJ_BOX) ok = hit_box(&w->items[i], origin, dir, &t, &n);
        else                              ok = hit_sphere(&w->items[i], origin, dir, &t, &n);
        if (!ok || t <= 0.0f) continue;
        if (t < best) {
            best = t;
            hit->t = t;
            hit->obj = i;
            hit->normal = n;
            found = 1;
        }
    }
    return found;
}

int world_save(const World *w, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# iota level\n");
    for (int i = 0; i < w->count; i++) {
        const Obj *o = &w->items[i];
        if (o->type == OBJ_BOX)
            fprintf(f, "box %.3f %.3f %.3f %.3f %.3f %.3f\n",
                    (double)o->pos.x, (double)o->pos.y, (double)o->pos.z,
                    (double)o->half.x, (double)o->half.y, (double)o->half.z);
        else
            fprintf(f, "sphere %.3f %.3f %.3f %.3f\n",
                    (double)o->pos.x, (double)o->pos.y, (double)o->pos.z,
                    (double)o->half.x);
    }
    fclose(f);
    return 0;
}

int world_load(World *w, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    world_init(w);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Obj o;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "box %f %f %f %f %f %f",
                   &o.pos.x, &o.pos.y, &o.pos.z,
                   &o.half.x, &o.half.y, &o.half.z) == 6) {
            o.type = OBJ_BOX;
            world_add(w, o);
        } else if (sscanf(line, "sphere %f %f %f %f",
                          &o.pos.x, &o.pos.y, &o.pos.z, &o.half.x) == 4) {
            o.half = v3(o.half.x, o.half.x, o.half.x);
            o.type = OBJ_SPHERE;
            world_add(w, o);
        }
    }
    fclose(f);
    return 0;
}

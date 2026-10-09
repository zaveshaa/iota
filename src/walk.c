#include "walk.h"

#include <math.h>
#include <stddef.h>

#define MAX_STEP (1.0f / 240.0f)

#define GRAVITY 18.0f
#define JUMP_VEL 6.7f
#define GROUND_ACCEL 60.0f
#define AIR_ACCEL 3.0f
#define AIR_WISH_CAP 6.0f
#define FRICTION 4.0f
#define FRICTION_STOP 16.0f
#define STOP_SPEED 0.25f

// how far past its edge a body may reach and still catch the top of a box: a
// body pressed against a face sits at exactly half + radius from the centre,
// so the catch wants a little room past that
#define EDGE 0.01f

// how long after the ground leaves the body the jump is still allowed, so a
// step off a ledge does not punish the jump that followed it
#define COYOTE 0.12f

static void apply_friction(Vec3 *vel, float dt, float rate, int stop)
{
    float speed = sqrtf(vel->x * vel->x + vel->z * vel->z);
    float newspeed;
    float k;

    if (speed < 1e-6f) {
        vel->x = 0.0f;
        vel->z = 0.0f;
        return;
    }
    newspeed = speed - speed * rate * dt;
    if (stop && newspeed < STOP_SPEED) {
        newspeed = 0.0f;
    }
    k = newspeed / speed;
    vel->x *= k;
    vel->z *= k;
}

static void accelerate(Vec3 *vel, Vec3 dir, float wishspeed, float accel,
                       float dt)
{
    float current = vel->x * dir.x + vel->z * dir.z;
    float add = wishspeed - current;
    float gained;

    if (add <= 0.0f) {
        return;
    }
    gained = accel * wishspeed * dt;
    if (gained > add) {
        gained = add;
    }
    vel->x += dir.x * gained;
    vel->z += dir.z * gained;
}

static void push_box(Vec3 *p, Vec3 center, Vec3 half, float radius,
                     float height, float step)
{
    float dx;
    float dz;

    if (center.y + half.y <= p->y + step) {
        return;
    }
    if (center.y - half.y >= p->y + height) {
        return;
    }
    dx = half.x + radius - fabsf(p->x - center.x);
    dz = half.z + radius - fabsf(p->z - center.z);
    if (dx <= 0.0f || dz <= 0.0f) {
        return;
    }
    if (dx < dz) {
        p->x += p->x < center.x ? -dx : dx;
    } else {
        p->z += p->z < center.z ? -dz : dz;
    }
}

// the box a mesh stands in, or 0 when the mesh is out of reach; the mesh is
// solid in the axis-aligned box around it, which its faces nearly fill
static int mesh_box(const Obj *o, const Meshes *ms, Vec3 *center, Vec3 *half)
{
    const Mesh *m;
    Vec3 mid;

    if (ms == NULL || o->mesh < 0 || o->mesh >= ms->count) {
        return 0;
    }
    m = &ms->items[o->mesh];
    mid = v3_mul(v3_add(m->lo, m->hi), 0.5f);
    *center = v3_add(o->pos, mid);
    *half = v3_mul(v3_sub(m->hi, m->lo), 0.5f);
    return 1;
}

static Vec3 depenetrate(const Scene *s, const Meshes *ms, Vec3 p, float radius,
                        float height, float step)
{
    Vec3 start = p;
    int i;

    for (i = 0; i < s->obj_count; i++) {
        const Obj *o = &s->objs[i];

        if (o->kind == OBJ_SPHERE) {
            float r = o->half.x + radius;
            float ex = p.x - o->pos.x;
            float ez = p.z - o->pos.z;
            float d = sqrtf(ex * ex + ez * ez);
            float k;

            if (o->pos.y + o->half.x <= p.y + step) {
                continue;
            }
            if (o->pos.y - o->half.x >= p.y + height) {
                continue;
            }
            if (d >= r) {
                continue;
            }
            if (d < 1e-4f) {
                p.x += r;
                continue;
            }
            k = (r - d) / d;
            p.x += ex * k;
            p.z += ez * k;
        } else if (o->kind == OBJ_BOX) {
            push_box(&p, o->pos, o->half, radius, height, step);
        } else if (o->kind == OBJ_MESH) {
            Vec3 center;
            Vec3 half;

            if (mesh_box(o, ms, &center, &half)) {
                push_box(&p, center, half, radius, height, step);
            }
        }
    }
    return v3_sub(p, start);
}

static float ground_scan(const Scene *s, const Meshes *ms, Vec3 p, float below,
                         float radius)
{
    float g = -1e30f;
    int i;

    for (i = 0; i < s->obj_count; i++) {
        const Obj *o = &s->objs[i];
        float top;

        if (o->kind == OBJ_SPHERE) {
            float r = o->half.x;
            float ex = p.x - o->pos.x;
            float ez = p.z - o->pos.z;
            float d2 = ex * ex + ez * ez;

            if (d2 >= r * r) {
                continue;
            }
            top = o->pos.y + sqrtf(r * r - d2);
        } else if (o->kind == OBJ_BOX) {
            if (fabsf(p.x - o->pos.x) > o->half.x + radius + EDGE) {
                continue;
            }
            if (fabsf(p.z - o->pos.z) > o->half.z + radius + EDGE) {
                continue;
            }
            top = o->pos.y + o->half.y;
        } else if (o->kind == OBJ_MESH) {
            Vec3 center;
            Vec3 half;

            if (!mesh_box(o, ms, &center, &half)) {
                continue;
            }
            if (fabsf(p.x - center.x) > half.x + radius + EDGE) {
                continue;
            }
            if (fabsf(p.z - center.z) > half.z + radius + EDGE) {
                continue;
            }
            top = center.y + half.y;
        } else if (o->axis.y > 0.5f) {
            top = o->pos.y;
        } else {
            continue;
        }
        if (top <= below && top > g) {
            g = top;
        }
    }
    return g;
}

static void walk_substep(const Scene *s, const Meshes *ms, Walk *w, Vec3 wish,
                         float speed, float dt)
{
    float len = sqrtf(wish.x * wish.x + wish.z * wish.z);
    Vec3 dir = v3(0.0f, 0.0f, 0.0f);
    Vec3 p;
    float ws = 0.0f;

    if (len > 1e-6f) {
        float k = 1.0f / len;

        dir = v3(wish.x * k, 0.0f, wish.z * k);
        ws = speed * (len < 1.0f ? len : 1.0f);
    }
    if (!w->on_ground && ws > AIR_WISH_CAP) {
        ws = AIR_WISH_CAP;
    }
    if (w->on_ground) {
        apply_friction(&w->vel, dt, ws > 0.0f ? FRICTION : FRICTION_STOP,
                       ws <= 0.0f);
        accelerate(&w->vel, dir, ws, GROUND_ACCEL, dt);
    } else {
        accelerate(&w->vel, dir, ws, AIR_ACCEL, dt);
    }
    if (w->on_ground) {
        w->coyote = COYOTE;
    } else if (w->coyote > 0.0f) {
        w->coyote -= dt;
    }
    if (w->jump && (w->on_ground || w->coyote > 0.0f)) {
        w->vel.y = JUMP_VEL;
        w->on_ground = 0;
        w->coyote = 0.0f;
        w->jump = 0;
    }
    w->vel.y -= GRAVITY * dt;

    p = w->pos;
    p.x += w->vel.x * dt;
    p.z += w->vel.z * dt;
    p = v3_add(p, depenetrate(s, ms, p, w->radius, w->height, w->step));

    p.y += w->vel.y * dt;
    {
        float ground = ground_scan(s, ms, p, p.y + w->step, w->radius);

        if (p.y <= ground) {
            p.y = ground;
            if (w->vel.y < 0.0f) {
                w->vel.y = 0.0f;
            }
            w->on_ground = 1;
        } else {
            w->on_ground = 0;
        }
    }
    p = v3_add(p, depenetrate(s, ms, p, w->radius, w->height, w->step));
    w->pos = p;
}

void walk_move(const Scene *s, const Meshes *ms, Walk *w, Vec3 wish,
               float speed, float dt)
{
    float left = dt;

    while (left > 0.0f) {
        float h = left > MAX_STEP ? MAX_STEP : left;

        walk_substep(s, ms, w, wish, speed, h);
        left -= h;
    }
}

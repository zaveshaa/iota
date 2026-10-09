#include "view.h"

#include <stdlib.h>
#include <time.h>

static const char g_ramp[] = VIEW_RAMP;

typedef struct {
    const Scene *s;
    const Meshes *ms;
    double deadline;
    int no_deadline;
    int expired;
    Vec3 fog;
    ViewStats *st;
} Ctx;

static double now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static Vec3 rgb_vec(unsigned c)
{
    return v3((float)((c >> 16) & 0xffu) / 255.0f,
              (float)((c >> 8) & 0xffu) / 255.0f,
              (float)(c & 0xffu) / 255.0f);
}

static float clamp01(float x)
{
    if (x < 0.0f) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

static unsigned byte_of(float x)
{
    return (unsigned)(clamp01(x) * 255.0f + 0.5f);
}

static float fog_factor(const Scene *s, float t)
{
    float f;

    if (s->fog_range <= 0.0f) {
        return 0.0f;
    }
    f = (t - s->fog_start) / s->fog_range;
    return clamp01(f);
}

static Vec3 mix(Vec3 a, Vec3 b, float f)
{
    return v3_add(v3_mul(a, 1.0f - f), v3_mul(b, f));
}

static Vec3 shade(const Scene *s, const Hit *h)
{
    Vec3 albedo = rgb_vec(h->obj->ink);
    Vec3 acc = v3_had(albedo, rgb_vec(s->ambient));
    int i;

    for (i = 0; i < s->light_count; i++) {
        const Light *l = &s->lights[i];
        Vec3 ld;
        float atten = 1.0f;
        float ndotl;

        if (l->kind == LIGHT_POINT) {
            Vec3 to = v3_sub(l->pos, h->point);
            float dist = v3_len(to);

            if (dist <= 1e-3f) {
                continue;
            }
            ld = v3_mul(to, 1.0f / dist);
            atten = 1.0f / (1.0f + 0.04f * dist * dist);
        } else {
            ld = v3_norm(v3_mul(l->dir, -1.0f));
        }
        ndotl = v3_dot(h->normal, ld);
        if (ndotl <= 0.0f) {
            continue;
        }
        acc = v3_add(acc,
                     v3_had(albedo, v3_mul(l->color,
                                           l->intensity * ndotl * atten)));
    }
    return acc;
}

static Vec3 trace(Ctx *cx, Vec3 origin, Vec3 dir, int depth)
{
    Hit h;
    Vec3 col;

    cx->st->rays++;
    if (depth > cx->st->depth_max) {
        cx->st->depth_max = depth;
    }
    if (!cx->no_deadline) {
        if (cx->expired) {
            return cx->fog;
        }
        if ((cx->st->rays & 63u) == 0u && now_ms() >= cx->deadline) {
            cx->expired = 1;
            cx->st->budget_hit = 1;
            return cx->fog;
        }
    }
    h = cast_ray(cx->s, cx->ms, origin, dir, 1e9f);
    if (h.action == HIT_NONE) {
        return cx->fog;
    }
    if (h.action == HIT_MIRROR && depth < VIEW_MAX_DEPTH) {
        Vec3 rd = v3_norm(v3_reflect(dir, h.normal));
        Vec3 ro = v3_add(h.point, v3_mul(h.normal, 2e-3f));

        col = trace(cx, ro, rd, depth + 1);
        return mix(col, cx->fog, fog_factor(cx->s, h.t));
    }
    if (h.action == HIT_MIRROR) {
        return cx->fog;
    }
    col = mix(shade(cx->s, &h), cx->fog, fog_factor(cx->s, h.t));
    return col;
}

void view_render(const Scene *s, const Meshes *ms, const Camera *cam, Render *r,
                 int ss, float budget_ms, ViewStats *st)
{
    static Vec3 *buf;
    static size_t cap;
    int cols = render_cols(r);
    int rows = render_rows(r);
    int w;
    int h;
    size_t need;
    Vec3 fogv = rgb_vec(s->fog);
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    Vec3 world_up = v3(0.0f, 1.0f, 0.0f);
    float tan_half;
    float aspect;
    float cp;
    float sp;
    Ctx cx;
    size_t i;
    int x;
    int y;

    if (ss < 1) {
        ss = 1;
    }
    if (ss > 2) {
        ss = 2;
    }
    w = cols * ss;
    h = rows * ss;
    need = (size_t)w * (size_t)h;
    if (need > cap) {
        Vec3 *nb = realloc(buf, need * sizeof *buf);

        if (nb == NULL) {
            return;
        }
        buf = nb;
        cap = need;
    }
    st->rays = 0u;
    st->depth_max = 0;
    st->budget_hit = 0;

    for (i = 0; i < need; i++) {
        buf[i] = fogv;
    }

    cp = cosf(cam->pitch);
    sp = sinf(cam->pitch);
    forward = v3(cp * sinf(cam->yaw), sp, cp * cosf(cam->yaw));
    right = v3_norm(v3_cross(forward, world_up));
    up = v3_cross(right, forward);
    tan_half = tanf(cam->fov * 0.5f);
    aspect = (float)w / (float)h;

    cx.s = s;
    cx.ms = ms;
    cx.no_deadline = budget_ms <= 0.0f;
    cx.deadline = cx.no_deadline ? 0.0 : now_ms() + (double)budget_ms;
    cx.expired = 0;
    cx.fog = fogv;
    cx.st = st;

    for (y = 0; y < h && !cx.expired; y++) {
        float py = (1.0f - 2.0f * ((float)y + 0.5f) / (float)h) * tan_half;

        for (x = 0; x < w; x++) {
            float px = (2.0f * ((float)x + 0.5f) / (float)w - 1.0f) *
                       aspect * tan_half;
            Vec3 dir = v3_norm(v3_add(forward,
                                      v3_add(v3_mul(right, px),
                                             v3_mul(up, py))));

            buf[(size_t)y * (size_t)w + (size_t)x] =
                trace(&cx, cam->pos, dir, 0);
        }
    }

    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            Vec3 c = v3(0.0f, 0.0f, 0.0f);
            float lum;
            unsigned char ch;
            unsigned ink;
            int sy;
            int sx;

            for (sy = 0; sy < ss; sy++) {
                for (sx = 0; sx < ss; sx++) {
                    size_t idx = (size_t)(y * ss + sy) * (size_t)w +
                                 (size_t)(x * ss + sx);

                    c = v3_add(c, buf[idx]);
                }
            }
            c = v3_mul(c, 1.0f / (float)(ss * ss));
            lum = c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
            {
                int level = (int)(clamp01(lum) * 10.0f);

                if (level > 9) {
                    level = 9;
                }
                ch = (unsigned char)g_ramp[level];
            }
            ink = (byte_of(c.x) << 16) | (byte_of(c.y) << 8) | byte_of(c.z);
            render_put(r, x, y, ch, ink);
        }
    }
}

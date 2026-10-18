#include "view.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

static const char g_ramp[] = VIEW_RAMP;

typedef struct {
    const Scene *s;
    const Meshes *ms;
    const Water *w;
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

// a shadow ray leaves the surface just off its face and asks whether anything
// stands between it and the light
static int in_shadow(const Scene *s, const Meshes *ms, Vec3 point, Vec3 normal,
                     Vec3 ld, float tmax)
{
    Vec3 ro = v3_add(point, v3_mul(normal, 2e-3f));
    Hit h = cast_ray(s, ms, ro, ld, tmax);

    return h.action != HIT_NONE;
}

static Vec3 shade(const Scene *s, const Meshes *ms, const Hit *h)
{
    Vec3 albedo = rgb_vec(h->obj->ink);
    Vec3 acc = v3_had(albedo, rgb_vec(s->ambient));
    int i;

    for (i = 0; i < s->light_count; i++) {
        const Light *l = &s->lights[i];
        Vec3 ld;
        float atten = 1.0f;
        float ndotl;
        float tmax = 1e9f;

        if (l->kind == LIGHT_POINT) {
            Vec3 to = v3_sub(l->pos, h->point);
            float dist = v3_len(to);

            if (dist <= 1e-3f) {
                continue;
            }
            ld = v3_mul(to, 1.0f / dist);
            atten = 1.0f / (1.0f + 0.04f * dist * dist);
            tmax = dist * 0.999f;
        } else {
            ld = v3_norm(v3_mul(l->dir, -1.0f));
        }
        ndotl = v3_dot(h->normal, ld);
        if (ndotl <= 0.0f) {
            continue;
        }
        if (in_shadow(s, ms, h->point, h->normal, ld, tmax)) {
            continue;
        }
        acc = v3_add(acc,
                     v3_had(albedo, v3_mul(l->color,
                                           l->intensity * ndotl * atten)));
    }
    return acc;
}

static float smoothstep01(float e0, float e1, float x)
{
    float t;

    if (e1 <= e0) {
        return x >= e1 ? 1.0f : 0.0f;
    }
    t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

static unsigned hash_u(unsigned x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// one star is offered per cell of a grid laid over the direction; a sparse
// hash keeps only a few, each a bright point about a pixel across
static float star_field(Vec3 dir)
{
    float px = dir.x * 48.0f;
    float py = dir.y * 48.0f;
    float pz = dir.z * 48.0f;
    int ix = (int)floorf(px);
    int iy = (int)floorf(py);
    int iz = (int)floorf(pz);
    unsigned h = hash_u((unsigned)ix * 73856093u ^
                        (unsigned)iy * 19349663u ^
                        (unsigned)iz * 83492791u);

    if ((h & 127u) != 0u) {
        return 0.0f;
    }
    {
        float a = (float)((h >> 3) & 0xffu) / 255.0f;
        float b = (float)((h >> 11) & 0xffu) / 255.0f;
        float c = (float)((h >> 19) & 0xffu) / 255.0f;
        float dx = px - ((float)ix + a);
        float dy = py - ((float)iy + b);
        float dz = pz - ((float)iz + c);
        float d2 = dx * dx + dy * dy + dz * dz;

        if (d2 < 0.6f) {
            float bright = (float)((h >> 27) & 0x1fu) / 31.0f;

            return (1.0f - d2 / 0.6f) * (0.5f + 0.5f * bright);
        }
    }
    return 0.0f;
}

static Vec3 sky_color(const Scene *s, Vec3 dir)
{
    float t = clamp01(dir.y);
    Vec3 c;

    t *= t;
    c = mix(rgb_vec(s->sky_horizon), rgb_vec(s->sky_top), t);
    if (s->have_moon) {
        Vec3 md = v3_norm(s->sky_moon_dir);
        float cd = v3_dot(dir, md);
        float r = s->sky_moon_size > 0.001f ? s->sky_moon_size : 0.05f;
        float ang;

        if (cd > 1.0f) {
            cd = 1.0f;
        }
        if (cd < -1.0f) {
            cd = -1.0f;
        }
        ang = acosf(cd);
        if (ang < r * 3.0f) {
            float g = 1.0f - smoothstep01(r, r * 3.0f, ang);

            c = v3_add(c, v3_mul(rgb_vec(s->sky_moon), g));
        }
    }
    if (dir.y > 0.12f) {
        float st = star_field(dir);

        if (st > 0.0f) {
            c = v3_add(c, v3(st, st, st * 1.05f));
        }
    }
    return c;
}

#define TILE_SIZE 0.8f
#define TILE_GROUT 0.06f
#define TILE_REFLECT 0.30f

static int face_axis(Vec3 n)
{
    float ax = fabsf(n.x);
    float ay = fabsf(n.y);
    float az = fabsf(n.z);

    if (ax >= ay && ax >= az) {
        return 0;
    }
    if (ay >= az) {
        return 1;
    }
    return 2;
}

static int on_grout(Vec3 p, Vec3 n)
{
    int axis = face_axis(n);
    float u = axis == 0 ? p.y : p.x;
    float v = axis == 2 ? p.y : p.z;
    float fu = u / TILE_SIZE - floorf(u / TILE_SIZE);
    float fv = v / TILE_SIZE - floorf(v / TILE_SIZE);
    float edge = TILE_GROUT / TILE_SIZE;

    return fu < edge || fu > 1.0f - edge || fv < edge || fv > 1.0f - edge;
}

static void plot_point(Render *r, Vec3 p, Vec3 eye, Vec3 fwd, Vec3 rt, Vec3 up,
                       float tan_half, float aspect, int cols, int rows,
                       unsigned char ch, unsigned ink)
{
    Vec3 rel = v3_sub(p, eye);
    float z = v3_dot(rel, fwd);
    float xr;
    float yu;
    int cx;
    int cy;

    if (z <= 0.05f) {
        return;
    }
    xr = v3_dot(rel, rt) / (z * aspect * tan_half);
    yu = v3_dot(rel, up) / (z * tan_half);
    if (xr < -1.0f || xr > 1.0f || yu < -1.0f || yu > 1.0f) {
        return;
    }
    cx = (int)((xr + 1.0f) * 0.5f * (float)cols);
    cy = (int)((1.0f - yu) * 0.5f * (float)rows);
    render_put(r, cx, cy, ch, ink);
}

static Vec3 water_color(float amp, Vec3 dir)
{
    float ndv = clamp01(fabsf(dir.y));
    float light = clamp01(0.25f + 0.45f * amp + 0.75f * (1.0f - ndv));

    if (light > 0.92f) {
        light = 0.92f;
    }
    {
        Vec3 deep = rgb_vec(RGB(24, 72, 168));
        Vec3 glint = rgb_vec(RGB(140, 205, 255));

        return v3_add(v3_mul(deep, 1.0f - light), v3_mul(glint, light));
    }
}

static Vec3 trace(Ctx *cx, Vec3 origin, Vec3 dir, int depth)
{
    Hit h;
    Vec3 col;
    int wp;
    float wt;
    float wa;
    Vec3 wn;

    cx->st->rays++;
    if (depth > cx->st->depth_max) {
        cx->st->depth_max = depth;
    }
    h = cast_ray(cx->s, cx->ms, origin, dir, 1e9f);
    if (cx->w != NULL) {
        float ndv;

        wp = water_cast(cx->w, origin, dir, h.t, &wt, &wn, &wa);
        if (wp >= 0) {
            ndv = clamp01(fabsf(dir.y));
            {
                float opacity = clamp01(0.55f + 0.20f * (1.0f - ndv));
                Vec3 behind = sky_color(cx->s, dir);

                if (h.action != HIT_NONE && h.obj != NULL) {
                    behind = mix(shade(cx->s, cx->ms, &h), cx->fog,
                                 fog_factor(cx->s, h.t));
                }
                return mix(mix(behind, water_color(wa, dir), opacity),
                           cx->fog, fog_factor(cx->s, wt));
            }
        }
    }
    if (h.action == HIT_NONE) {
        return sky_color(cx->s, dir);
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
    col = mix(shade(cx->s, cx->ms, &h), cx->fog, fog_factor(cx->s, h.t));
    if (h.action == HIT_TILE) {
        if (on_grout(h.point, h.normal)) {
            col = v3_mul(col, 0.18f);
        } else if (depth < VIEW_MAX_DEPTH) {
            Vec3 rd = v3_norm(v3_reflect(dir, h.normal));
            Vec3 ro = v3_add(h.point, v3_mul(h.normal, 2e-3f));
            Vec3 refl = trace(cx, ro, rd, depth + 1);

            col = mix(col, refl, TILE_REFLECT);
        }
    }
    return col;
}

void view_render(const Scene *s, const Meshes *ms, const Water *w,
                 const Camera *cam, Render *r, int ss, float budget_ms,
                 ViewStats *st)
{
    static Vec3 *buf;
    static size_t cap;
    static int *plev;
    static size_t pcap;
    int cols = render_cols(r);
    int rows = render_rows(r);
    int wsize;
    int h;
    size_t need;
    double started = now_ms();
    Vec3 fogv = rgb_vec(s->fog);
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    Vec3 world_up = v3(0.0f, 1.0f, 0.0f);
    float tan_half;
    float aspect;
    float cp;
    float spitch;
    Ctx cx;
    size_t i;
    int x;
    int y;

    if (ss < 1) {
        ss = 1;
    }
    if (ss > VIEW_SS_MAX) {
        ss = VIEW_SS_MAX;
    }
    wsize = cols * ss;
    h = rows * ss;
    need = (size_t)wsize * (size_t)h;
    if (need > cap) {
        Vec3 *nb = realloc(buf, need * sizeof *buf);

        if (nb == NULL) {
            return;
        }
        buf = nb;
        cap = need;
    }
    if (need > pcap) {
        int *np = calloc(need, sizeof *np);

        if (np == NULL) {
            return;
        }
        plev = np;
        pcap = need;
    }
    st->rays = 0u;
    st->depth_max = 0;
    st->budget_hit = 0;

    for (i = 0; i < need; i++) {
        buf[i] = fogv;
    }

    cp = cosf(cam->pitch);
    spitch = sinf(cam->pitch);
    forward = v3(cp * sinf(cam->yaw), spitch, cp * cosf(cam->yaw));
    right = v3_norm(v3_cross(forward, world_up));
    up = v3_cross(right, forward);
    tan_half = tanf(cam->fov * 0.5f);
    aspect = (float)wsize / (float)h;

    cx.s = s;
    cx.ms = ms;
    cx.w = w;
    cx.fog = fogv;
    cx.st = st;

    for (y = 0; y < h; y++) {
        float py = (1.0f - 2.0f * ((float)y + 0.5f) / (float)h) * tan_half;

        for (x = 0; x < wsize; x++) {
            float px = (2.0f * ((float)x + 0.5f) / (float)wsize - 1.0f) *
                       aspect * tan_half;
            Vec3 dir = v3_norm(v3_add(forward,
                                      v3_add(v3_mul(right, px),
                                             v3_mul(up, py))));

            buf[(size_t)y * (size_t)wsize + (size_t)x] =
                trace(&cx, cam->pos, dir, 0);
        }
    }
    st->budget_hit = budget_ms > 0.0f &&
                     now_ms() - started > (double)budget_ms;

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
                    size_t idx = (size_t)(y * ss + sy) * (size_t)wsize +
                                 (size_t)(x * ss + sx);

                    c = v3_add(c, buf[idx]);
                }
            }
            c = v3_mul(c, 1.0f / (float)(ss * ss));
            lum = c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
            {
                /* keep a glyph until its light clears the next ramp step, so an
                   animated surface does not churn character to character */
                size_t gi = (size_t)y * (size_t)cols + (size_t)x;
                int lev = plev[gi];
                int raw = (int)(clamp01(lum) * 10.0f);

                if (raw > 9) {
                    raw = 9;
                }
                if (raw > lev && lum > (float)(lev + 1) * 0.10f + 0.03f) {
                    lev = raw;
                } else if (raw < lev &&
                           lum < (float)lev * 0.10f - 0.03f) {
                    lev = raw;
                }
                plev[gi] = lev;
                ch = (unsigned char)g_ramp[lev];
            }
            ink = (byte_of(c.x) << 16) | (byte_of(c.y) << 8) | byte_of(c.z);
            render_put(r, x, y, ch, ink);
        }
    }

    if (w != NULL) {
        int k;

        for (k = 0; k < w->spout_n; k++) {
            const Spout *sp = &w->spouts[k];
            float sy = water_surface(w, sp->top.x, sp->top.z);
            unsigned ink = RGB(120, 178, 236);
            int j;

            for (j = 0; j <= 12; j++) {
                float f = (float)j / 12.0f;
                Vec3 p = v3(sp->top.x, sp->top.y + (sy - sp->top.y) * f,
                            sp->top.z);

                plot_point(r, p, cam->pos, forward, right, up, tan_half,
                           aspect, cols, rows, (unsigned char)'|', ink);
            }
        }
        for (k = 0; k < w->steam_n; k++) {
            const Steam *puff = &w->steam[k];
            float f = puff->life / puff->span;
            unsigned char ch;
            unsigned ink;
            int v;

            if (f < 0.18f) {
                continue;
            }
            ch = f > 0.6f ? (unsigned char)'`'
                          : (f > 0.35f ? (unsigned char)'.'
                                       : (unsigned char)',');
            v = (int)(255.0f * (0.35f + 0.65f * f));
            ink = RGB(v * 190 / 255, v * 214 / 255, v * 245 / 255);
            plot_point(r, puff->p, cam->pos, forward, right, up, tan_half,
                       aspect, cols, rows, ch, ink);
        }
    }
}

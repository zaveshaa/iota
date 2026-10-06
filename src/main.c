#include "camera.h"
#include "render.h"
#include "term.h"
#include "vec3.h"
#include "world.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define FOV      1.1f
#define FOV_MIN  0.18f
#define FOV_MAX  1.6f
#define SS       2
#define SS_SLOW  15
#define SS_FAST  8
#define FRAME_MS 10
#define STEP     0.12f
#define WALK     0.08f
#define TURN     0.03f
#define TURN_OBJ 0.2618f   // 15 degrees on r and shift+r
#define PITCH_MAX 1.5f
#define REACH    4.0f
#define EYE      1.7f
#define GRAV     0.15f
#define BODY     0.25f
#define STEP_UP  0.7f

typedef enum { MODE_FLY, MODE_WALK } Mode;

static const char *LEVEL_PATH = "level.txt";

static long ms_between(struct timespec a, struct timespec b) {
    return (a.tv_sec - b.tv_sec) * 1000 + (a.tv_nsec - b.tv_nsec) / 1000000;
}

// what one ray sees, sky writes RENDER_FAR
static char shade_ray(Camera cam, const World *w, const Meshes *ms, Vec3 dir, Vec3 light, Vec3 fwd, float *t) {
    Hit best = {RENDER_FAR, -1, v3(0.0f, 1.0f, 0.0f)};

    if (dir.y < -0.0001f) {
        float tf = -cam.pos.y / dir.y;
        if (tf > 0.0f && tf < best.t) { best.t = tf; best.obj = -1; }
    }

    Hit oh;
    if (world_cast(w, ms, cam.pos, dir, 120.0f, &oh) && oh.t < best.t) best = oh;

    *t = best.t;
    if (best.t >= RENDER_FAR * 0.5f) return RENDER_SKY;

    float fog = 1.0f - best.t / 32.0f;
    if (fog < 0.0f) fog = 0.0f;

    if (best.obj < 0) {
        // floor: shade with fog, cross the grid with '|'
        Vec3 hit = v3_add(cam.pos, v3_scale(dir, best.t));
        float gx = __builtin_fabsf(hit.x - __builtin_roundf(hit.x));
        float gz = __builtin_fabsf(hit.z - __builtin_roundf(hit.z));
        float g = gx < gz ? gx : gz;
        return (g > 0.015f && g < 0.035f) ? '|' : RENDER_RAMP[render_shade(fog * 0.7f)];
    }

    // lambert from above plus a fill from the viewer, times fog
    float lam = v3_dot(best.normal, light);
    if (lam < 0.0f) lam = 0.0f;
    float face = -v3_dot(best.normal, fwd);
    if (face < 0.0f) face = 0.0f;
    return RENDER_RAMP[render_shade((0.22f + 0.40f * lam + 0.38f * face) * fog)];
}

static void draw_scene(Camera cam, const World *w, const Meshes *ms, float fov, int ss, int cols, int rows) {
    Vec3 fwd, right, up;
    cam_basis(cam, &fwd, &right, &up);

    Vec3 light = v3_norm(v3(0.45f, 0.85f, 0.3f));
    float aspect = (float)cols / (float)rows * 0.5f;
    float dxs = 2.0f / (float)(cols - 1) * aspect / (float)ss;
    float dys = 2.0f / (float)(rows - 1) / (float)ss;

    for (int row = 0; row < rows; row++) {
        float sy0 = 1.0f - 2.0f * (float)row / (float)(rows - 1);
        for (int col = 0; col < cols; col++) {
            float sx0 = (2.0f * (float)col / (float)(cols - 1) - 1.0f) * aspect;

            // ss x ss rays per cell, the closest sample wins
            float bt = RENDER_FAR;
            char ch = RENDER_SKY;
            for (int j = 0; j < ss; j++) {
                float sy = sy0 + ((float)j - (float)(ss - 1) * 0.5f) * dys;
                for (int i = 0; i < ss; i++) {
                    float sx = sx0 + ((float)i - (float)(ss - 1) * 0.5f) * dxs;
                    Vec3 dir = v3_norm(v3_add(fwd, v3_add(v3_scale(right, sx * fov), v3_scale(up, sy * fov))));
                    float t;
                    char c = shade_ray(cam, w, ms, dir, light, fwd, &t);
                    if (t < bt) { bt = t; ch = c; }
                }
            }

            render_put(col, row, ch, bt);
        }
    }

    render_put(cols / 2, rows / 2, '+', -1.0f);
}

// push the body out of everything it overlaps, on the horizontal plane
static void push_out(const World *w, const Meshes *ms, Vec3 *p) {
    float feet = p->y - EYE;
    for (int i = 0; i < w->count; i++) {
        const Obj *o = &w->items[i];

        if (o->type == OBJ_SPHERE) {
            float r = o->half.x * o->scale + BODY;
            float ex = p->x - o->pos.x, ez = p->z - o->pos.z;
            float d = __builtin_sqrtf(ex * ex + ez * ez);
            if (d >= r) continue;
            if (d < 0.0001f) { p->x += r; continue; }
            float k = (r - d) / d;
            p->x += ex * k;
            p->z += ez * k;
            continue;
        }

        Vec3 lo, hi;
        if (!world_local_box(w, ms, o, &lo, &hi)) continue;

        // low enough to just step on top of
        if (o->pos.y + hi.y * o->scale <= feet + STEP_UP) continue;

        // the test runs in the object frame, so a turned box has no fat corners
        Vec3 lp = world_to_local(o, *p);
        float body = BODY / o->scale;
        float cx = (lo.x + hi.x) * 0.5f, cz = (lo.z + hi.z) * 0.5f;
        float dx = (hi.x - lo.x) * 0.5f + body - __builtin_fabsf(lp.x - cx);
        float dz = (hi.z - lo.z) * 0.5f + body - __builtin_fabsf(lp.z - cz);
        if (dx <= 0.0f || dz <= 0.0f) continue;
        // leave by the nearest face
        if (dx < dz) lp.x += (lp.x < cx ? -dx : dx);
        else         lp.z += (lp.z < cz ? -dz : dz);
        *p = world_from_local(o, lp);
    }
}

// highest surface under p, the floor counts as zero
static float support(const World *w, const Meshes *ms, Vec3 p) {
    float g = 0.0f;
    for (int i = 0; i < w->count; i++) {
        const Obj *o = &w->items[i];
        float top;

        if (o->type == OBJ_SPHERE) {
            float r = o->half.x * o->scale;
            float ex = p.x - o->pos.x, ez = p.z - o->pos.z;
            float d2 = ex * ex + ez * ez;
            if (d2 >= r * r) continue;
            top = o->pos.y + __builtin_sqrtf(r * r - d2);
        } else {
            Vec3 lo, hi;
            if (!world_local_box(w, ms, o, &lo, &hi)) continue;
            Vec3 lp = world_to_local(o, p);
            if (lp.x < lo.x || lp.x > hi.x || lp.z < lo.z || lp.z > hi.z) continue;
            top = o->pos.y + hi.y * o->scale;
        }

        if (top > g) g = top;
    }
    return g;
}

static void step_walk(Camera *cam, const World *w, const Meshes *ms, Vec3 fwd, Vec3 right,
                      float ahead, float strafe) {
    Vec3 flat;
    if (__builtin_fabsf(fwd.x) < 0.001f && __builtin_fabsf(fwd.z) < 0.001f)
        flat = v3(__builtin_sinf(cam->yaw), 0.0f, __builtin_cosf(cam->yaw));
    else
        flat = v3_norm(v3(fwd.x, 0.0f, fwd.z));

    Vec3 p = v3_add(cam->pos, v3_add(v3_scale(flat, ahead), v3_scale(right, strafe)));

    push_out(w, ms, &p);

    float ground = support(w, ms, p) + EYE;
    if (p.y > ground) {
        float drop = p.y - ground;
        p.y -= drop < GRAV ? drop : GRAV;
    } else {
        p.y = ground;
    }

    cam->pos = p;
}

// returns 0 to quit, 1 when the view has to be redrawn, -1 when nothing happened
static int apply(Camera *cam, World *w, Meshes *ms, ObjType *type, int *pick, Mode *mode, float *fov, Key k) {
    if (k == KEY_EOF) return 0;
    if (k == KEY_NONE) return -1;

    Vec3 fwd, right, up;
    cam_basis(*cam, &fwd, &right, &up);

    switch (k) {
        case KEY_W:
            if (*mode == MODE_FLY) cam->pos = v3_add(cam->pos, v3_scale(fwd, STEP));
            else                  step_walk(cam, w, ms, fwd, right, WALK, 0.0f);
            break;
        case KEY_S:
            if (*mode == MODE_FLY) cam->pos = v3_sub(cam->pos, v3_scale(fwd, STEP));
            else                  step_walk(cam, w, ms, v3_scale(fwd, -1.0f), right, WALK, 0.0f);
            break;
        case KEY_A:
            if (*mode == MODE_FLY) cam->pos = v3_sub(cam->pos, v3_scale(right, STEP));
            else                  step_walk(cam, w, ms, fwd, right, 0.0f, -WALK);
            break;
        case KEY_D:
            if (*mode == MODE_FLY) cam->pos = v3_add(cam->pos, v3_scale(right, STEP));
            else                  step_walk(cam, w, ms, fwd, right, 0.0f, WALK);
            break;
        case KEY_TURN_L: cam->yaw -= TURN; break;
        case KEY_TURN_R: cam->yaw += TURN; break;
        case KEY_TURN_U: cam->pitch += TURN; break;
        case KEY_TURN_D: cam->pitch -= TURN; break;

        case KEY_TAB:    *mode = (*mode == MODE_FLY) ? MODE_WALK : MODE_FLY; break;
        case KEY_BOX:    *type = OBJ_BOX; break;
        case KEY_SPHERE: *type = OBJ_SPHERE; break;
        case KEY_MESH:   *type = OBJ_MESH; break;

        case KEY_TURN_OBJ_L: case KEY_TURN_OBJ_R: case KEY_SCALE: {
            Hit h;
            if (!world_cast(w, ms, cam->pos, fwd, REACH * 6.0f, &h) || h.obj < 0) break;
            Obj *o = &w->items[h.obj];

            if (k == KEY_SCALE) {
                o->scale = o->scale >= 3.9f ? 0.5f : o->scale * 2.0f;
            } else {
                o->yaw += (k == KEY_TURN_OBJ_R ? TURN_OBJ : -TURN_OBJ);
                if (o->yaw < 0.0f) o->yaw += 6.2831853f;
                if (o->yaw >= 6.2831853f) o->yaw -= 6.2831853f;
            }
            break;
        }

        case KEY_ZOOM_IN:  if (*fov > FOV_MIN) *fov -= 0.08f; break;
        case KEY_ZOOM_OUT: if (*fov < FOV_MAX) *fov += 0.08f; break;

        case KEY_PREV:
            if (*pick > 0) (*pick)--;
            break;
        case KEY_NEXT:
            if (*pick + 1 < ms->count) (*pick)++;
            break;

        case KEY_PLACE: {
            // straight ahead, resting on the floor
            Vec3 p = v3_add(cam->pos, v3_scale(fwd, REACH));
            Obj o = {v3(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f), -1, *type, 0.0f, 1.0f};
            if (*type == OBJ_BOX) {
                o.half = v3(0.4f, 0.4f, 0.4f);
                o.pos = v3(p.x, o.half.y, p.z);
            } else if (*type == OBJ_SPHERE) {
                o.half = v3(0.5f, 0.5f, 0.5f);
                o.pos = v3(p.x, o.half.x, p.z);
            } else {
                if (ms->count == 0) break;
                o.mesh = *pick;
                o.pos = v3(p.x, 0.0f, p.z);
            }
            world_add(w, o);
            break;
        }

        case KEY_DEL: {
            Hit h;
            if (world_cast(w, ms, cam->pos, fwd, 24.0f, &h) && h.obj >= 0)
                world_remove(w, h.obj);
            break;
        }

        case KEY_SAVE: world_save(w, ms, LEVEL_PATH); break;
        case KEY_LOAD: world_load(w, ms, LEVEL_PATH); break;

        case KEY_ESC: return 0;
        default: break;
    }

    if (cam->pitch > PITCH_MAX)  cam->pitch = PITCH_MAX;
    if (cam->pitch < -PITCH_MAX) cam->pitch = -PITCH_MAX;
    return 1;
}

static void draw_hud(const World *w, const Meshes *ms, Camera cam, ObjType type, int pick,
                     Mode mode, float fov, int rows) {
    char what[64];
    if (type == OBJ_BOX)        snprintf(what, sizeof(what), "box");
    else if (type == OBJ_SPHERE) snprintf(what, sizeof(what), "sphere");
    else if (ms->count)         snprintf(what, sizeof(what), "%s", ms->items[pick].name);
    else                        snprintf(what, sizeof(what), "no meshes");

    char hud[256];
    snprintf(hud, sizeof(hud),
             " [tab] %s  [= -] zoom %.2f  [1] box  [2] sphere  [3] mesh (,/.) = %s   [space] place  [x] del  [o] save  [l] load  n=%d ",
             mode == MODE_FLY ? "fly" : "walk", fov, what, w->count);
    render_text(0, rows - 1, hud);

    if (rows < 3) return;

    // what the crosshair is on, so r and y are discoverable without a manual
    Vec3 fwd, right, up;
    cam_basis(cam, &fwd, &right, &up);

    char line[128], name[40];
    Hit h;
    if (world_cast(w, ms, cam.pos, fwd, REACH * 6.0f, &h) && h.obj >= 0) {
        const Obj *o = &w->items[h.obj];
        if (o->type == OBJ_MESH && ms && o->mesh >= 0 && o->mesh < ms->count)
            snprintf(name, sizeof(name), "%s", ms->items[o->mesh].name);
        else
            snprintf(name, sizeof(name), o->type == OBJ_BOX ? "box" : "sphere");
        snprintf(line, sizeof(line), " %s  yaw %d  size x%.1f", name,
                 (int)(o->yaw * 57.2958f) % 360, o->scale);
    } else {
        line[0] = 0;
    }
    snprintf(line + strlen(line), sizeof(line) - strlen(line),
             "    [r]/[R] turn 15  [y] size 0.5/1/2/4");
    render_text(0, rows - 2, line);
}

int main(void) {
    term_init();

    Camera cam = {v3(0.0f, 1.7f, -6.0f), 0.0f, 0.0f};
    ObjType type = OBJ_BOX;
    Mode mode = MODE_FLY;
    int pick = 0;
    float fov = FOV;

    static Meshes meshes;
    meshes.count = 0;
    mesh_list(&meshes, "meshes.txt");

    World world;
    world_init(&world);
    world_add(&world, (Obj){v3(0.0f, 0.4f, 4.0f), v3(0.4f, 0.4f, 0.4f), -1, OBJ_BOX, 0.0f, 1.0f});
    world_add(&world, (Obj){v3(2.0f, 0.5f, 6.0f), v3(0.5f, 0.5f, 0.5f), -1, OBJ_SPHERE, 0.0f, 1.0f});

    int cols = term_cols(), rows = term_rows();
    render_init(cols, rows);

    struct timespec drawn = {0, 0}, mark;
    int ss = SS;
    for (;;) {
        int changed = apply(&cam, &world, &meshes, &type, &pick, &mode, &fov, term_read_key());
        if (changed == 0) break;

        if (term_dirty()) {
            int c = term_cols(), r = term_rows();
            if (c != cols || r != rows) {
                render_resize(c, r);
                cols = c;
                rows = r;
            }
            term_clean();
            changed = 1;
        }

        // no input and no resize, so there is nothing new to draw
        if (changed < 0) continue;

        clock_gettime(CLOCK_MONOTONIC, &mark);

        render_clear();
        draw_scene(cam, &world, &meshes, fov, ss, cols, rows);
        draw_hud(&world, &meshes, cam, type, pick, mode, fov, rows);
        render_present();

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // supersampling is the first thing to go when a frame gets slow
        long cost = ms_between(now, mark);
        if (cost > SS_SLOW) ss = 1;
        else if (cost < SS_FAST) ss = SS;

        // a flood of input gets throttled here, anything slower than a
        // hundred keys a second is never held back
        long gap = ms_between(now, drawn);
        if (gap < FRAME_MS) {
            struct timespec rest = {0, (FRAME_MS - gap) * 1000000L};
            nanosleep(&rest, NULL);
            clock_gettime(CLOCK_MONOTONIC, &drawn);
        } else {
            drawn = now;
        }
    }

    render_free();
    term_close();
    return 0;
}

#include "camera.h"
#include "render.h"
#include "term.h"
#include "vec3.h"
#include "world.h"

#include <stdio.h>

#define FOV    1.1f
#define STEP   0.12f
#define WALK   0.08f
#define TURN   0.03f
#define PITCH_MAX 1.5f
#define REACH  4.0f
#define EYE    1.7f
#define GRAV   0.15f
#define BODY   0.25f
#define STEP_UP 0.7f

typedef enum { MODE_FLY, MODE_WALK } Mode;

static const char *LEVEL_PATH = "level.txt";

static void draw_scene(Camera cam, const World *w, const Meshes *ms, int cols, int rows) {
    Vec3 fwd, right, up;
    cam_basis(cam, &fwd, &right, &up);

    Vec3 light = v3_norm(v3(0.45f, 0.85f, 0.3f));
    float aspect = (float)cols / (float)rows * 0.5f;

    for (int row = 0; row < rows; row++) {
        float sy = 1.0f - 2.0f * (float)row / (float)(rows - 1);
        for (int col = 0; col < cols; col++) {
            // one ray per cell
            float sx = (2.0f * (float)col / (float)(cols - 1) - 1.0f) * aspect;
            Vec3 dir = v3_norm(v3_add(fwd, v3_add(v3_scale(right, sx * FOV), v3_scale(up, sy * FOV))));

            Hit best = {RENDER_FAR, -1, v3(0.0f, 1.0f, 0.0f)};

            if (dir.y < -0.0001f) {
                float tf = -cam.pos.y / dir.y;
                if (tf > 0.0f && tf < best.t) { best.t = tf; best.obj = -1; }
            }

            Hit oh;
            if (world_cast(w, ms, cam.pos, dir, 120.0f, &oh) && oh.t < best.t) best = oh;

            if (best.t >= RENDER_FAR * 0.5f) {
                render_put(col, row, RENDER_SKY, RENDER_FAR);
                continue;
            }

            float fog = 1.0f - best.t / 32.0f;
            if (fog < 0.0f) fog = 0.0f;

            if (best.obj < 0) {
                // floor: shade with fog, cross the grid with '|'
                Vec3 hit = v3_add(cam.pos, v3_scale(dir, best.t));
                float gx = __builtin_fabsf(hit.x - __builtin_roundf(hit.x));
                float gz = __builtin_fabsf(hit.z - __builtin_roundf(hit.z));
                float g = gx < gz ? gx : gz;
                char ch = (g > 0.015f && g < 0.035f) ? '|' : RENDER_RAMP[render_shade(fog * 0.7f)];
                render_put(col, row, ch, best.t);
            } else {
                // lambert against the light, times fog
                float d = v3_dot(best.normal, light);
                if (d < 0.0f) d = 0.0f;
                float s = (0.35f + 0.65f * d) * fog;
                render_put(col, row, RENDER_RAMP[render_shade(s)], best.t);
            }
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
            float r = o->half.x + BODY;
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
        if (!world_bounds(w, ms, o, &lo, &hi)) continue;

        // low enough to just step on top of
        if (hi.y <= feet + STEP_UP) continue;

        float cx = (lo.x + hi.x) * 0.5f, cz = (lo.z + hi.z) * 0.5f;
        float dx = (hi.x - lo.x) * 0.5f + BODY - __builtin_fabsf(p->x - cx);
        float dz = (hi.z - lo.z) * 0.5f + BODY - __builtin_fabsf(p->z - cz);
        if (dx <= 0.0f || dz <= 0.0f) continue;
        // leave by the nearest face
        if (dx < dz) p->x += (p->x < cx ? -dx : dx);
        else         p->z += (p->z < cz ? -dz : dz);
    }
}

// highest surface under p, the floor counts as zero
static float support(const World *w, const Meshes *ms, Vec3 p) {
    float g = 0.0f;
    for (int i = 0; i < w->count; i++) {
        const Obj *o = &w->items[i];
        float top;

        if (o->type == OBJ_SPHERE) {
            float r = o->half.x;
            float ex = p.x - o->pos.x, ez = p.z - o->pos.z;
            float d2 = ex * ex + ez * ez;
            if (d2 >= r * r) continue;
            top = o->pos.y + __builtin_sqrtf(r * r - d2);
        } else {
            Vec3 lo, hi;
            if (!world_bounds(w, ms, o, &lo, &hi)) continue;
            if (p.x < lo.x || p.x > hi.x || p.z < lo.z || p.z > hi.z) continue;
            top = hi.y;
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

static int handle(Camera *cam, World *w, Meshes *ms, ObjType *type, int *pick, Mode *mode) {
    Key k = term_read_key();
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

        case KEY_PREV:
            if (*pick > 0) (*pick)--;
            break;
        case KEY_NEXT:
            if (*pick + 1 < ms->count) (*pick)++;
            break;

        case KEY_PLACE: {
            // straight ahead, resting on the floor
            Vec3 p = v3_add(cam->pos, v3_scale(fwd, REACH));
            Obj o = {v3(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f), -1, *type};
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

static void draw_hud(const World *w, const Meshes *ms, ObjType type, int pick, Mode mode, int rows) {
    char what[64];
    if (type == OBJ_BOX)        snprintf(what, sizeof(what), "box");
    else if (type == OBJ_SPHERE) snprintf(what, sizeof(what), "sphere");
    else if (ms->count)         snprintf(what, sizeof(what), "%s", ms->items[pick].name);
    else                        snprintf(what, sizeof(what), "no meshes");

    char hud[256];
    snprintf(hud, sizeof(hud),
             " [tab] %s  [1] box  [2] sphere  [3] mesh (,/.) = %s   [space] place  [x] del  [o] save  [l] load  n=%d ",
             mode == MODE_FLY ? "fly" : "walk", what, w->count);
    render_text(0, rows - 1, hud);
}

int main(void) {
    term_init();

    Camera cam = {v3(0.0f, 1.7f, -6.0f), 0.0f, 0.0f};
    ObjType type = OBJ_BOX;
    Mode mode = MODE_FLY;
    int pick = 0;

    static Meshes meshes;
    meshes.count = 0;
    mesh_list(&meshes, "meshes.txt");

    World world;
    world_init(&world);
    world_add(&world, (Obj){v3(0.0f, 0.4f, 4.0f), v3(0.4f, 0.4f, 0.4f), -1, OBJ_BOX});
    world_add(&world, (Obj){v3(2.0f, 0.5f, 6.0f), v3(0.5f, 0.5f, 0.5f), -1, OBJ_SPHERE});

    int cols = term_cols(), rows = term_rows();
    render_init(cols, rows);

    for (;;) {
        if (term_dirty()) {
            int c = term_cols(), r = term_rows();
            if (c != cols || r != rows) {
                render_resize(c, r);
                cols = c;
                rows = r;
            }
            term_clean();
        }

        if (!handle(&cam, &world, &meshes, &type, &pick, &mode)) break;

        render_clear();
        draw_scene(cam, &world, &meshes, cols, rows);
        draw_hud(&world, &meshes, type, pick, mode, rows);
        render_present();
    }

    render_free();
    term_close();
    return 0;
}

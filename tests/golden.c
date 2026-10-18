#include "scene.h"
#include "view.h"
#include "water.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOLD_COLS 80
#define GOLD_ROWS 24
#define MAX_REPORTS 8

static int g_checks;
static int g_failures;

static void check(int ok, const char *name, const char *detail)
{
    g_checks++;
    if (ok) {
        (void)printf("ok   %s\n", name);
        return;
    }
    g_failures++;
    (void)printf("FAIL %s%s%s\n", name, detail != NULL ? " - " : "",
                 detail != NULL ? detail : "");
}

static int ramp_index(unsigned char ch)
{
    const char *at = strchr(VIEW_RAMP, (char)ch);

    if (at == NULL) {
        return -1;
    }
    return (int)(at - VIEW_RAMP);
}

static int near_enough(unsigned char got, unsigned char want)
{
    int a = ramp_index(got);
    int b = ramp_index((unsigned char)want);
    int diff = a - b;

    if (a < 0 || b < 0) {
        return 0;
    }
    if (diff < 0) {
        diff = -diff;
    }
    return diff <= 1;
}

static int ink_near(unsigned got, unsigned want)
{
    int i;

    for (i = 0; i < 3; i++) {
        int shift = i * 8;
        int g = (int)((got >> shift) & 0xffu);
        int w = (int)((want >> shift) & 0xffu);
        int d = g - w;

        if (d < 0) {
            d = -d;
        }
        if (d > 3) {
            return 0;
        }
    }
    return 1;
}

static void cast_checks(const Scene *s)
{
    char detail[128];
    Hit h;
    Vec3 rd;

    h = cast_ray(s, NULL, v3(0.0f, 2.0f, 0.0f), v3(0.0f, -1.0f, 0.0f), 100.0f);
    check(h.action == HIT_CONTINUE, "a ray down hits the floor", NULL);
    (void)snprintf(detail, sizeof detail, "t is %.3f, wanted 2.000",
                   (double)h.t);
    check(fabsf(h.t - 2.0f) < 1e-3f, "the floor lies two units down",
          detail);
    (void)snprintf(detail, sizeof detail, "normal is (%.3f, %.3f, %.3f)",
                   (double)h.normal.x, (double)h.normal.y,
                   (double)h.normal.z);
    check(v3_dot(h.normal, v3(0.0f, 1.0f, 0.0f)) > 0.99f,
          "the floor normal points up", detail);

    h = cast_ray(s, NULL, v3(-0.5f, 0.7f, 2.8f), v3(1.0f, 0.0f, 0.0f), 100.0f);
    check(h.action == HIT_MIRROR, "the sphere asks for a bounce", NULL);
    (void)snprintf(detail, sizeof detail, "normal is (%.3f, %.3f, %.3f)",
                   (double)h.normal.x, (double)h.normal.y,
                   (double)h.normal.z);
    check(h.normal.x < -0.99f, "the mirror normal faces the ray", detail);
    rd = v3_reflect(v3(1.0f, 0.0f, 0.0f), h.normal);
    (void)snprintf(detail, sizeof detail, "bounced dir is (%.3f, %.3f, %.3f)",
                   (double)rd.x, (double)rd.y, (double)rd.z);
    check(rd.x < -0.99f, "the ray bounces back the way it came", detail);

    h = cast_ray(s, NULL, v3(0.0f, 1.7f, -1.4f), v3(0.0f, 0.0f, 1.0f), 100.0f);
    (void)snprintf(detail, sizeof detail, "normal is (%.3f, %.3f, %.3f)",
                   (double)h.normal.x, (double)h.normal.y,
                   (double)h.normal.z);
    check(h.action == HIT_MIRROR && h.normal.z < -0.99f,
          "the wall holds a mirror facing the room", detail);

    h = cast_ray(s, NULL, v3(0.0f, 5.0f, 0.0f), v3(0.0f, 1.0f, 0.0f), 100.0f);
    check(h.action == HIT_NONE, "a ray up meets only sky", NULL);

    h = cast_ray(s, NULL, v3(-2.4f, 1.0f, 1.0f), v3(0.0f, 0.0f, 1.0f), 100.0f);
    check(h.action == HIT_CONTINUE && h.obj != NULL &&
              h.obj->ink == RGB(224, 144, 64),
          "a ray at the pillar finds the orange box", NULL);

    h = cast_ray(s, NULL, v3(0.0f, 1.5f, 5.0f),
                 v3_norm(v3(0.0f, -1.5f, 5.0f)), 100.0f);
    (void)snprintf(detail, sizeof detail, "hit z is %.3f", (double)h.point.z);
    check(h.point.z > 8.0f && h.point.z < 8.7f,
          "the wall occludes the floor behind it", detail);
}

static void yaw_checks(void)
{
    Scene s;
    Obj *o;
    Hit h;

    scene_clear(&s);
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 0.0f, 0.0f);
    o->half = v3(2.0f, 1.0f, 0.2f);

    h = cast_ray(&s, NULL, v3(0.0f, 0.0f, 3.0f), v3(0.0f, 0.0f, -1.0f), 100.0f);
    check(h.action == HIT_CONTINUE && fabsf(h.point.z - 0.2f) < 1e-3f,
          "a long box is hit on its wide face", NULL);

    o->yaw = 1.57079632679f;
    h = cast_ray(&s, NULL, v3(0.0f, 0.0f, 3.0f), v3(0.0f, 0.0f, -1.0f), 100.0f);
    check(h.action == HIT_CONTINUE && fabsf(h.point.z - 2.0f) < 1e-3f,
          "turned a quarter, the long face meets the front ray", NULL);

    h = cast_ray(&s, NULL, v3(3.0f, 0.0f, 0.0f), v3(-1.0f, 0.0f, 0.0f), 100.0f);
    check(h.action == HIT_CONTINUE && fabsf(h.point.x - 0.2f) < 1e-3f &&
              h.normal.x > 0.99f,
          "turned, its narrow face meets a ray from the side", NULL);
}

static void tile_checks(void)
{
    Scene s;
    Obj *o;
    Hit h;

    scene_clear(&s);
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 0.0f, 0.0f);
    o->half = v3(1.0f, 1.0f, 1.0f);
    o->flags = OBJ_TILE;

    h = cast_ray(&s, NULL, v3(0.0f, 3.0f, 0.0f), v3(0.0f, -1.0f, 0.0f), 100.0f);
    check(h.action == HIT_TILE, "a tiled floor asks to be a faint mirror", NULL);

    o->flags = OBJ_NONE;
    h = cast_ray(&s, NULL, v3(0.0f, 3.0f, 0.0f), v3(0.0f, -1.0f, 0.0f), 100.0f);
    check(h.action == HIT_CONTINUE, "an untiled floor is a plain surface", NULL);
}

static float frame_lum(const Scene *s, const Camera *cam)
{
    Render *r = render_open(32, 16);
    ViewStats st;
    float sum = 0.0f;
    int x;
    int y;

    if (r == NULL) {
        return 0.0f;
    }
    view_render(s, NULL, NULL, cam, r, 1, 0.0f, &st);
    for (y = 0; y < render_rows(r); y++) {
        for (x = 0; x < render_cols(r); x++) {
            unsigned ink = render_ink(r, x, y);

            sum += (float)(((ink >> 16) & 0xffu) + ((ink >> 8) & 0xffu) +
                           (ink & 0xffu));
        }
    }
    render_close(r);
    return sum;
}

static void shadow_checks(void)
{
    Scene s;
    Camera cam;
    Obj *o;
    Light *l;
    float open_lum;
    float blocked_lum;

    scene_clear(&s);
    s.ambient = 0u;
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, -0.5f, 0.0f);
    o->half = v3(20.0f, 0.5f, 20.0f);
    o->ink = RGB(200, 200, 200);
    l = scene_add_light(&s);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 6.0f, 0.0f);
    l->color = v3(1.0f, 1.0f, 1.0f);
    l->intensity = 3.0f;
    cam.pos = v3(0.0f, 5.0f, 6.0f);
    cam.yaw = 3.14159265f;
    cam.pitch = -0.6f;
    cam.fov = 1.05f;

    open_lum = frame_lum(&s, &cam);

    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 3.0f, 0.0f);
    o->half = v3(1.5f, 0.2f, 1.5f);
    o->ink = 0u;

    blocked_lum = frame_lum(&s, &cam);
    check(open_lum > 0.0f && blocked_lum < open_lum,
          "a blocker darkens the floor it stands over", NULL);
}

#define STEAM_COLS 48
#define STEAM_ROWS 20

static void render_grid(const Scene *s, const Water *w, const Camera *cam,
                        unsigned char *ch, unsigned *ink)
{
    Render *r = render_open(STEAM_COLS, STEAM_ROWS);
    ViewStats st;
    int x;
    int y;

    if (r == NULL) {
        return;
    }
    view_render(s, NULL, w, cam, r, 1, 0.0f, &st);
    for (y = 0; y < STEAM_ROWS; y++) {
        for (x = 0; x < STEAM_COLS; x++) {
            ch[(size_t)y * STEAM_COLS + (size_t)x] = render_char(r, x, y);
            ink[(size_t)y * STEAM_COLS + (size_t)x] = render_ink(r, x, y);
        }
    }
    render_close(r);
}

static int grids_same(const unsigned char *ca, const unsigned *ia,
                      const unsigned char *cb, const unsigned *ib)
{
    size_t i;

    for (i = 0; i < (size_t)STEAM_COLS * STEAM_ROWS; i++) {
        if (ca[i] != cb[i] || ia[i] != ib[i]) {
            return 0;
        }
    }
    return 1;
}

static void steam_checks(void)
{
    static unsigned char ca[STEAM_COLS * STEAM_ROWS];
    static unsigned char cb[STEAM_COLS * STEAM_ROWS];
    static unsigned ia[STEAM_COLS * STEAM_ROWS];
    static unsigned ib[STEAM_COLS * STEAM_ROWS];
    Scene s;
    Water w;
    Water quiet;
    Camera cam;
    Obj *o;
    int i;

    scene_clear(&s);
    s.ambient = RGB(40, 44, 54);
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, -0.6f, 0.0f);
    o->half = v3(20.0f, 0.5f, 20.0f);
    o->ink = RGB(150, 150, 150);

    (void)memset(&w, 0, sizeof w);
    water_open(&w, v3(-3.0f, -1.0f, -3.0f), v3(3.0f, -1.0f, 3.0f), -0.5f,
               0.5f);
    w.heat = 1.0f;
    water_spout(&w, v3(0.0f, 1.6f, 0.0f), 3.0f);
    for (i = 0; i < 150; i++) {
        water_step(&w, 1.0f / 60.0f);
    }
    quiet = w;
    quiet.steam_n = 0;

    cam.pos = v3(0.0f, 1.6f, 7.0f);
    cam.yaw = 3.14159265f;
    cam.pitch = 0.0f;
    cam.fov = 1.05f;

    render_grid(&s, &w, &cam, ca, ia);
    render_grid(&s, &quiet, &cam, cb, ib);
    check(!grids_same(ca, ia, cb, ib),
          "clear of a wall, the steam shows", NULL);

    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 3.5f, 3.5f);
    o->half = v3(6.0f, 6.0f, 0.2f);
    o->ink = RGB(90, 90, 90);

    render_grid(&s, &w, &cam, ca, ia);
    render_grid(&s, &quiet, &cam, cb, ib);
    check(grids_same(ca, ia, cb, ib),
          "a wall keeps the steam out of sight", NULL);

    water_close(&w);
}

static int read_golden(const char *path, unsigned *want)
{
    FILE *f = fopen(path, "rb");
    int y;

    if (f == NULL) {
        return -1;
    }
    for (y = 0; y < GOLD_ROWS; y++) {
        char line[1024];
        int x;

        if (fgets(line, sizeof line, f) == NULL) {
            (void)fclose(f);
            return -1;
        }
        for (x = 0; x < GOLD_COLS; x++) {
            unsigned v = 0u;
            const char *tok = line + x * 9;

            if (sscanf(tok, "%8x", &v) != 1) {
                (void)fclose(f);
                return -1;
            }
            want[(size_t)y * GOLD_COLS + (size_t)x] = v;
        }
    }
    (void)fclose(f);
    return 0;
}

static int write_golden(const char *path, const Render *r)
{
    FILE *f = fopen(path, "wb");
    int y;

    if (f == NULL) {
        return -1;
    }
    for (y = 0; y < GOLD_ROWS; y++) {
        int x;

        for (x = 0; x < GOLD_COLS; x++) {
            unsigned v = ((unsigned)render_char(r, x, y) << 24) |
                         render_ink(r, x, y);
            (void)fprintf(f, "%08x%c", v, x + 1 == GOLD_COLS ? '\n' : ' ');
        }
    }
    (void)fclose(f);
    return 0;
}

static void shoji_checks(void)
{
    static unsigned char ca[STEAM_COLS * STEAM_ROWS];
    static unsigned char cb[STEAM_COLS * STEAM_ROWS];
    static unsigned ia[STEAM_COLS * STEAM_ROWS];
    static unsigned ib[STEAM_COLS * STEAM_ROWS];
    Scene s;
    Camera cam;
    Obj *o;
    Light *l;

    cam.pos = v3(0.0f, 1.6f, -4.0f);
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    cam.fov = 1.05f;

    // a screen that fills the view, with a lit wall behind it: the paper lets
    // a ghost of what stands behind filter through, so taking the light away
    // dims the panes
    scene_clear(&s);
    s.ambient = RGB(24, 26, 34);
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 1.6f, 0.0f);
    o->half = v3(20.0f, 12.0f, 0.1f);
    o->ink = RGB(224, 214, 192);
    o->flags = OBJ_SHOJI;
    o = scene_add_obj(&s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 1.6f, 3.0f);
    o->half = v3(20.0f, 12.0f, 0.5f);
    o->ink = RGB(120, 170, 255);
    l = scene_add_light(&s);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 1.6f, 2.0f);
    l->color = v3(0.6f, 0.8f, 1.0f);
    l->intensity = 8.0f;
    render_grid(&s, NULL, &cam, ca, ia);
    s.light_count = 0;
    render_grid(&s, NULL, &cam, cb, ib);
    check(!grids_same(ca, ia, cb, ib),
          "light behind a shoji screen filters through its paper", NULL);

    // the same screen as a plain wall keeps what stands behind it out of
    // sight: the back light never reaches the near face
    s.objs[0].flags = 0u;
    s.light_count = 1;
    render_grid(&s, NULL, &cam, ca, ia);
    s.light_count = 0;
    render_grid(&s, NULL, &cam, cb, ib);
    check(grids_same(ca, ia, cb, ib),
          "a plain wall keeps what stands behind it out of sight", NULL);
}

int main(int argc, char **argv)
{
    static unsigned want[GOLD_COLS * GOLD_ROWS];
    Scene scene;
    Camera cam;
    ViewStats st;
    Render *r;
    int writing = argc > 1 && strcmp(argv[1], "--write") == 0;
    const char *path = writing && argc > 2  ? argv[2]
                       : argc > 1           ? argv[1]
                                            : "tests/golden/mirror.txt";
    int bad = 0;
    int reports = 0;
    int y;
    int x;

    mirror_scene(&scene, &cam);
    cast_checks(&scene);
    tile_checks();
    yaw_checks();
    shadow_checks();
    steam_checks();
    shoji_checks();

    r = render_open(GOLD_COLS, GOLD_ROWS);
    if (r == NULL) {
        (void)fprintf(stderr, "golden: no memory for the frame\n");
        return 1;
    }
    view_render(&scene, NULL, NULL, &cam, r, 1, 0.0f, &st);
    check(st.depth_max >= 1 && st.depth_max <= VIEW_MAX_DEPTH,
          "mirrors bounce within the depth we promised", NULL);
    check(st.rays > (unsigned)(GOLD_COLS * GOLD_ROWS) &&
              st.rays < (unsigned)(GOLD_COLS * GOLD_ROWS) * 3u,
          "the mirror adds rays without doubling them", NULL);

    {
        Render *again = render_open(GOLD_COLS, GOLD_ROWS);
        ViewStats st2;

        if (again == NULL) {
            (void)fprintf(stderr, "golden: no memory for the frame\n");
            return 1;
        }
        view_render(&scene, NULL, NULL, &cam, again, 1, 0.0f, &st2);
        {
            int same = 1;

            for (y = 0; y < GOLD_ROWS && same; y++) {
                for (x = 0; x < GOLD_COLS; x++) {
                    if (render_char(r, x, y) != render_char(again, x, y) ||
                        render_ink(r, x, y) != render_ink(again, x, y)) {
                        same = 0;
                        break;
                    }
                }
            }
            check(same, "the same scene renders the same twice", NULL);
        }
        render_close(again);
    }

    if (writing) {
        if (write_golden(path, r) != 0) {
            (void)fprintf(stderr, "golden: cannot write %s\n", path);
            return 1;
        }
        (void)printf("wrote %s\n", path);
        render_close(r);
        return 0;
    }

    if (read_golden(path, want) != 0) {
        (void)fprintf(stderr, "golden: cannot read %s\n", path);
        return 1;
    }
    for (y = 0; y < GOLD_ROWS; y++) {
        for (x = 0; x < GOLD_COLS; x++) {
            unsigned got = ((unsigned)render_char(r, x, y) << 24) |
                           render_ink(r, x, y);
            unsigned w = want[(size_t)y * GOLD_COLS + (size_t)x];
            char detail[96];

            if (near_enough((unsigned char)(got >> 24),
                            (unsigned char)(w >> 24)) &&
                ink_near(got & 0xffffffu, w & 0xffffffu)) {
                continue;
            }
            bad++;
            if (reports < MAX_REPORTS) {
                reports++;
                (void)snprintf(detail, sizeof detail,
                               "at (%d,%d) got %08x want %08x", x, y, got, w);
                (void)printf("     %s\n", detail);
            }
        }
    }
    {
        char detail[64];

        (void)snprintf(detail, sizeof detail, "%d cells differ", bad);
        check(bad == 0, "the frame matches the frame it should be", detail);
    }

    render_close(r);
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

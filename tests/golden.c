#include "scene.h"
#include "view.h"

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

    h = cast_ray(s, v3(0.0f, 2.0f, 0.0f), v3(0.0f, -1.0f, 0.0f), 100.0f);
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

    h = cast_ray(s, v3(-1.0f, 1.0f, 4.5f), v3(1.0f, 0.0f, 0.0f), 100.0f);
    check(h.action == HIT_MIRROR, "the sphere asks for a bounce", NULL);
    (void)snprintf(detail, sizeof detail, "normal is (%.3f, %.3f, %.3f)",
                   (double)h.normal.x, (double)h.normal.y,
                   (double)h.normal.z);
    check(h.normal.x < -0.99f, "the mirror normal faces the ray", detail);
    rd = v3_reflect(v3(1.0f, 0.0f, 0.0f), h.normal);
    (void)snprintf(detail, sizeof detail, "bounced dir is (%.3f, %.3f, %.3f)",
                   (double)rd.x, (double)rd.y, (double)rd.z);
    check(rd.x < -0.99f, "the ray bounces back the way it came", detail);

    h = cast_ray(s, v3(0.0f, 5.0f, 0.0f), v3(0.0f, 1.0f, 0.0f), 100.0f);
    check(h.action == HIT_NONE, "a ray up meets only sky", NULL);

    h = cast_ray(s, v3(-2.0f, 1.0f, 2.0f), v3(0.0f, 0.0f, 1.0f), 100.0f);
    check(h.action == HIT_CONTINUE && h.obj != NULL &&
              h.obj->ink == RGB(224, 144, 64),
          "a ray at the pillar finds the orange box", NULL);

    h = cast_ray(s, v3(0.0f, 1.5f, 5.0f),
                 v3_norm(v3(0.0f, -1.5f, 5.0f)), 100.0f);
    (void)snprintf(detail, sizeof detail, "hit z is %.3f", (double)h.point.z);
    check(h.point.z > 8.0f && h.point.z < 8.7f,
          "the wall occludes the floor behind it", detail);
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

    r = render_open(GOLD_COLS, GOLD_ROWS);
    if (r == NULL) {
        (void)fprintf(stderr, "golden: no memory for the frame\n");
        return 1;
    }
    view_render(&scene, &cam, r, 1, 0.0f, &st);
    check(st.depth_max == 1, "the mirror bounces exactly once", NULL);
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
        view_render(&scene, &cam, again, 1, 0.0f, &st2);
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

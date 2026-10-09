#include "mesh.h"

#include <math.h>
#include <stdio.h>

static int g_checks;
static int g_failures;

static void check(int ok, const char *name)
{
    g_checks++;
    if (ok) {
        (void)printf("ok   %s\n", name);
        return;
    }
    g_failures++;
    (void)printf("FAIL %s\n", name);
}

static void tri(Mesh *m, Vec3 a, Vec3 b, Vec3 c)
{
    if (!mesh_add(m, a, b, c)) {
        (void)fprintf(stderr, "meshtest: out of memory\n");
    }
}

static void cube(Mesh *m)
{
    Vec3 v[8];

    v[0] = v3(-0.5f, -0.5f, -0.5f);
    v[1] = v3(0.5f, -0.5f, -0.5f);
    v[2] = v3(0.5f, 0.5f, -0.5f);
    v[3] = v3(-0.5f, 0.5f, -0.5f);
    v[4] = v3(-0.5f, -0.5f, 0.5f);
    v[5] = v3(0.5f, -0.5f, 0.5f);
    v[6] = v3(0.5f, 0.5f, 0.5f);
    v[7] = v3(-0.5f, 0.5f, 0.5f);

    tri(m, v[0], v[1], v[2]);
    tri(m, v[0], v[2], v[3]);
    tri(m, v[4], v[6], v[5]);
    tri(m, v[4], v[7], v[6]);
    tri(m, v[0], v[3], v[7]);
    tri(m, v[0], v[7], v[4]);
    tri(m, v[1], v[5], v[6]);
    tri(m, v[1], v[6], v[2]);
    tri(m, v[0], v[4], v[5]);
    tri(m, v[0], v[5], v[1]);
    tri(m, v[3], v[2], v[6]);
    tri(m, v[3], v[6], v[7]);
}

static void tree_hits(void)
{
    Mesh m;
    float t = 0.0f;
    Vec3 n = v3(0.0f, 0.0f, 0.0f);

    mesh_init(&m, "cube");
    cube(&m);
    check(mesh_build(&m), "a cube of triangles builds a tree");

    check(mesh_tri_hit(&m, v3(0.0f, 0.0f, -5.0f), v3(0.0f, 0.0f, 1.0f), &t, &n) &&
              fabsf(t - 4.5f) < 1e-3f,
          "a ray to the front of the cube stops on it");

    check(mesh_tri_hit(&m, v3(0.0f, 5.0f, 0.0f), v3(0.0f, -1.0f, 0.0f), &t, &n) &&
              fabsf(t - 4.5f) < 1e-3f,
          "a ray down the cube lands on its top");

    check(!mesh_tri_hit(&m, v3(0.0f, 0.0f, -5.0f), v3(0.0f, 0.0f, -1.0f),
                        &t, &n),
          "a ray that turns away misses the cube");

    check(!mesh_tri_hit(&m, v3(5.0f, 5.0f, -5.0f), v3(0.0f, 0.0f, 1.0f), &t, &n),
          "a ray beside the cube passes it");

    mesh_free(&m);
}

static void empty_mesh(void)
{
    Mesh m;
    float t = 0.0f;
    Vec3 n = v3(0.0f, 0.0f, 0.0f);

    mesh_init(&m, "empty");
    check(!mesh_tri_hit(&m, v3(0.0f, 0.0f, -1.0f), v3(0.0f, 0.0f, 1.0f), &t,
                        &n),
          "a mesh with no tree is never hit");
}

static void obj_loads(void)
{
    static const char *text =
        "v -1 0 -1\n"
        "v 1 0 -1\n"
        "v 1 0 1\n"
        "v -1 0 1\n"
        "f 1 2 3\n"
        "f 1 3 4\n";
    const char *path = "meshtest_tmp.obj";
    Meshes ms;
    FILE *f = fopen(path, "w");
    float t = 0.0f;
    Vec3 n = v3(0.0f, 0.0f, 0.0f);
    int idx;

    if (f == NULL) {
        check(0, "the loader reads a face from a file");
        return;
    }
    (void)fputs(text, f);
    fclose(f);

    ms.count = 0;
    check(mesh_load(&ms, path) == 0, "the loader reads a face from a file");
    idx = mesh_find(&ms, "meshtest_tmp");
    check(idx == 0, "a loaded mesh is found by its name");
    check(idx >= 0 &&
              mesh_tri_hit(&ms.items[idx], v3(0.0f, 5.0f, 0.0f),
                           v3(0.0f, -1.0f, 0.0f), &t, &n) &&
              fabsf(t - 5.0f) < 1e-3f,
          "a ray hits the face the file described");
    mesh_free(&ms.items[idx]);
    remove(path);

    check(mesh_load(&ms, "no_such_mesh.obj") < 0,
          "a missing file loads as nothing");
}

int main(void)
{
    tree_hits();
    empty_mesh();
    obj_loads();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

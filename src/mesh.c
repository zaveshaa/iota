#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The centroid of every triangle, rebuilt for each tree and shared by the splits
// so a node never has to walk its own slice twice.
static Vec3 *g_cent;

static int tri_reserve(Mesh *m)
{
    int cap;
    Tri *t;

    if (m->count < m->cap) {
        return 1;
    }
    cap = m->cap != 0 ? m->cap * 2 : 1024;
    t = realloc(m->tris, sizeof(Tri) * (size_t)cap);
    if (t == NULL) {
        return 0;
    }
    m->tris = t;
    m->cap = cap;
    return 1;
}

static int node_new(Mesh *m)
{
    int cap;
    MeshNode *n;

    if (m->nodes_count == m->nodes_cap) {
        cap = m->nodes_cap != 0 ? m->nodes_cap * 2 : 256;
        n = realloc(m->nodes, sizeof(MeshNode) * (size_t)cap);
        if (n == NULL) {
            return -1;
        }
        m->nodes = n;
        m->nodes_cap = cap;
    }
    return m->nodes_count++;
}

void mesh_init(Mesh *m, const char *path)
{
    const char *slash;
    char *dot;

    memset(m, 0, sizeof *m);
    m->lo = v3(1e30f, 1e30f, 1e30f);
    m->hi = v3(-1e30f, -1e30f, -1e30f);

    slash = strrchr(path, '/');
    (void)snprintf(m->name, sizeof m->name, "%s",
                   slash != NULL ? slash + 1 : path);
    dot = strchr(m->name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
}

int mesh_add(Mesh *m, Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 vs[3];
    int i;

    if (!tri_reserve(m)) {
        return 0;
    }
    m->tris[m->count].a = a;
    m->tris[m->count].b = b;
    m->tris[m->count].c = c;
    m->count++;

    vs[0] = a;
    vs[1] = b;
    vs[2] = c;
    for (i = 0; i < 3; i++) {
        if (vs[i].x < m->lo.x) {
            m->lo.x = vs[i].x;
        }
        if (vs[i].y < m->lo.y) {
            m->lo.y = vs[i].y;
        }
        if (vs[i].z < m->lo.z) {
            m->lo.z = vs[i].z;
        }
        if (vs[i].x > m->hi.x) {
            m->hi.x = vs[i].x;
        }
        if (vs[i].y > m->hi.y) {
            m->hi.y = vs[i].y;
        }
        if (vs[i].z > m->hi.z) {
            m->hi.z = vs[i].z;
        }
    }
    return 1;
}

static float axis_val(Vec3 c, int axis)
{
    return axis == 0 ? c.x : (axis == 1 ? c.y : c.z);
}

static float pick(int tri, int axis)
{
    return axis_val(g_cent[tri], axis);
}

// Hoare split on the axis, everything below the pivot brought to the front.
static void partition(Mesh *m, int start, int count, int axis, float pivot,
                      int *k)
{
    int i = start;
    int j = start + count - 1;

    while (i <= j) {
        while (i <= j && pick(m->order[i], axis) < pivot) {
            i++;
        }
        while (i <= j && pick(m->order[j], axis) > pivot) {
            j--;
        }
        if (i <= j) {
            int t = m->order[i];

            m->order[i] = m->order[j];
            m->order[j] = t;
            i++;
            j--;
        }
    }
    *k = i - start;
}

// The tree is built on the longest centroid axis, split at the midpoint.
static int build(Mesh *m, int start, int count)
{
    Vec3 lo = v3(1e30f, 1e30f, 1e30f);
    Vec3 hi = v3(-1e30f, -1e30f, -1e30f);
    Vec3 clo = lo;
    Vec3 chi = hi;
    int ni = node_new(m);
    int i;

    if (ni < 0) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        const Tri *t = &m->tris[m->order[start + i]];
        Vec3 vs[3];
        Vec3 c;
        int k;

        vs[0] = t->a;
        vs[1] = t->b;
        vs[2] = t->c;
        c = v3_mul(v3_add(v3_add(t->a, t->b), t->c), 1.0f / 3.0f);
        if (c.x < clo.x) {
            clo.x = c.x;
        }
        if (c.y < clo.y) {
            clo.y = c.y;
        }
        if (c.z < clo.z) {
            clo.z = c.z;
        }
        if (c.x > chi.x) {
            chi.x = c.x;
        }
        if (c.y > chi.y) {
            chi.y = c.y;
        }
        if (c.z > chi.z) {
            chi.z = c.z;
        }
        for (k = 0; k < 3; k++) {
            if (vs[k].x < lo.x) {
                lo.x = vs[k].x;
            }
            if (vs[k].y < lo.y) {
                lo.y = vs[k].y;
            }
            if (vs[k].z < lo.z) {
                lo.z = vs[k].z;
            }
            if (vs[k].x > hi.x) {
                hi.x = vs[k].x;
            }
            if (vs[k].y > hi.y) {
                hi.y = vs[k].y;
            }
            if (vs[k].z > hi.z) {
                hi.z = vs[k].z;
            }
        }
    }
    m->nodes[ni].lo = lo;
    m->nodes[ni].hi = hi;

    if (count <= MESH_LEAF) {
        m->nodes[ni].left = -1;
        m->nodes[ni].right = -1;
        m->nodes[ni].start = start;
        m->nodes[ni].count = count;
        return ni;
    }
    {
        float cx = chi.x - clo.x;
        float cy = chi.y - clo.y;
        float cz = chi.z - clo.z;
        int axis = (cx > cy && cx > cz) ? 0 : (cy > cz ? 1 : 2);
        float pivot = (axis_val(clo, axis) + axis_val(chi, axis)) * 0.5f;
        int k = 0;
        int l;
        int r;

        partition(m, start, count, axis, pivot, &k);
        if (k <= 0 || k >= count) {
            k = count / 2;
        }
        l = build(m, start, k);
        r = build(m, start + k, count - k);
        // the recursion grows the node array, so the index is written through
        m->nodes[ni].left = l;
        m->nodes[ni].right = r;
        m->nodes[ni].count = 0;
    }
    return ni;
}

int mesh_build(Mesh *m)
{
    int i;
    int root;

    free(m->order);
    m->order = malloc(sizeof(int) * (size_t)m->count);
    g_cent = malloc(sizeof(Vec3) * (size_t)m->count);
    if (m->order == NULL || g_cent == NULL) {
        free(g_cent);
        g_cent = NULL;
        return 0;
    }
    for (i = 0; i < m->count; i++) {
        m->order[i] = i;
        g_cent[i] = v3_mul(v3_add(v3_add(m->tris[i].a, m->tris[i].b),
                                 m->tris[i].c),
                           1.0f / 3.0f);
    }
    m->nodes_count = 0;
    root = build(m, 0, m->count);
    free(g_cent);
    g_cent = NULL;
    return root >= 0;
}

void mesh_free(Mesh *m)
{
    free(m->tris);
    free(m->nodes);
    free(m->order);
    memset(m, 0, sizeof *m);
}

// Positions first, then every face fanned into triangles. A negative index is
// counted back from the positions read so far, and any index a face names that is
// not in the file is dropped rather than trusted, because a malformed model is
// worth what parses and nothing more.
int mesh_load(Meshes *ms, const char *path)
{
    FILE *f;
    Mesh *m;
    Vec3 *verts;
    int vcap = 1024;
    int vn = 0;
    char line[256];

    if (ms->count >= MESH_MAX) {
        return -1;
    }
    f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    m = &ms->items[ms->count];
    mesh_init(m, path);
    verts = malloc(sizeof(Vec3) * (size_t)vcap);
    if (verts == NULL) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof line, f) != NULL) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x;
            float y;
            float z;

            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) != 3) {
                continue;
            }
            if (vn == vcap) {
                Vec3 *nv;

                vcap *= 2;
                nv = realloc(verts, sizeof(Vec3) * (size_t)vcap);
                if (nv == NULL) {
                    free(verts);
                    fclose(f);
                    return -1;
                }
                verts = nv;
            }
            verts[vn++] = v3(x, y, z);
        } else if (line[0] == 'f' && line[1] == ' ') {
            int idx[FACE_MAX];
            int n = 0;
            char *p = line + 2;
            int i;

            while (n < FACE_MAX) {
                long k;

                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                if ((*p < '0' || *p > '9') && *p != '-') {
                    break;
                }
                k = strtol(p, &p, 10);
                if (k == 0) {
                    break;
                }
                idx[n++] = (int)k;
                // step over any /vt and /vn the face also names
                while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                    p++;
                }
            }
            for (i = 0; i < n; i++) {
                idx[i] = idx[i] > 0 ? idx[i] - 1 : vn + idx[i];
            }
            for (i = 1; i + 1 < n; i++) {
                if (idx[0] < 0 || idx[0] >= vn) {
                    break;
                }
                if (idx[i] < 0 || idx[i] >= vn) {
                    continue;
                }
                if (idx[i + 1] < 0 || idx[i + 1] >= vn) {
                    continue;
                }
                if (!mesh_add(m, verts[idx[0]], verts[idx[i]],
                              verts[idx[i + 1]])) {
                    free(verts);
                    fclose(f);
                    mesh_free(m);
                    return -1;
                }
            }
        }
    }

    free(verts);
    fclose(f);

    if (m->count == 0 || !mesh_build(m)) {
        mesh_free(m);
        return -1;
    }
    return ms->count++;
}

// One path per line, with blank lines and # skipped.
int mesh_list(Meshes *ms, const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];

    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        char *p = line;
        size_t n;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        n = strlen(p);
        while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r' ||
                         p[n - 1] == ' ')) {
            p[--n] = '\0';
        }
        if (n == 0 || *p == '#') {
            continue;
        }
        (void)mesh_load(ms, p);
    }
    fclose(f);
    return ms->count;
}

int mesh_find(const Meshes *ms, const char *name)
{
    int i;

    for (i = 0; i < ms->count; i++) {
        if (strcmp(ms->items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// A slab test that reports where the ray enters the box.
static int slab(Vec3 lo, Vec3 hi, Vec3 o, Vec3 d, float tmax, float *tout)
{
    float t0 = 0.0f;
    float t1 = tmax;
    float loa[3] = {lo.x, lo.y, lo.z};
    float hia[3] = {hi.x, hi.y, hi.z};
    float oa[3] = {o.x, o.y, o.z};
    float da[3] = {d.x, d.y, d.z};
    int i;

    for (i = 0; i < 3; i++) {
        float inv;
        float a;
        float b;

        if (__builtin_fabsf(da[i]) < 1e-9f) {
            if (oa[i] < loa[i] || oa[i] > hia[i]) {
                return 0;
            }
            continue;
        }
        inv = 1.0f / da[i];
        a = (loa[i] - oa[i]) * inv;
        b = (hia[i] - oa[i]) * inv;
        if (a > b) {
            float tmp = a;

            a = b;
            b = tmp;
        }
        if (a > t0) {
            t0 = a;
        }
        if (b < t1) {
            t1 = b;
        }
        if (t0 > t1) {
            return 0;
        }
    }
    *tout = t0;
    return 1;
}

// Moller-Trumbore, reporting the distance to the hit.
static int tri_hit(const Tri *t, Vec3 o, Vec3 d, float tmax, float *tout)
{
    Vec3 e1 = v3_sub(t->b, t->a);
    Vec3 e2 = v3_sub(t->c, t->a);
    Vec3 pv = v3_cross(d, e2);
    float det = v3_dot(e1, pv);
    float inv;
    Vec3 tv;
    float u;
    Vec3 qv;
    float v;
    float tt;

    if (__builtin_fabsf(det) < 1e-9f) {
        return 0;
    }
    inv = 1.0f / det;
    tv = v3_sub(o, t->a);
    u = v3_dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) {
        return 0;
    }
    qv = v3_cross(tv, e1);
    v = v3_dot(d, qv) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return 0;
    }
    tt = v3_dot(e2, qv) * inv;
    if (tt <= 0.0f || tt > tmax) {
        return 0;
    }
    *tout = tt;
    return 1;
}

static void bvh_walk(const Mesh *m, int ni, Vec3 o, Vec3 d, float *best,
                     int *hit, Vec3 *n)
{
    float entry;
    int i;

    if (!slab(m->nodes[ni].lo, m->nodes[ni].hi, o, d, *best, &entry)) {
        return;
    }
    if (m->nodes[ni].left < 0) {
        for (i = 0; i < m->nodes[ni].count; i++) {
            const Tri *t =
                &m->tris[m->order[m->nodes[ni].start + i]];
            float tt;

            if (!tri_hit(t, o, d, *best, &tt)) {
                continue;
            }
            {
                Vec3 e1 = v3_sub(t->b, t->a);
                Vec3 e2 = v3_sub(t->c, t->a);

                *best = tt;
                *n = v3_norm(v3_cross(e1, e2));
                *hit = 1;
            }
        }
        return;
    }
    bvh_walk(m, m->nodes[ni].left, o, d, best, hit, n);
    bvh_walk(m, m->nodes[ni].right, o, d, best, hit, n);
}

int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n)
{
    float best = 1e30f;
    int hit = 0;
    Vec3 bn = v3(0.0f, 1.0f, 0.0f);

    if (m->nodes_count == 0) {
        return 0;
    }
    bvh_walk(m, 0, o, d, &best, &hit, &bn);
    if (hit) {
        *t = best;
        *n = bn;
    }
    return hit;
}

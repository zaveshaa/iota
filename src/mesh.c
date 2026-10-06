#include "mesh.h"

#include "gltf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int tri_reserve(Mesh *m) {
    if (m->count < m->cap) return 1;
    int cap = m->cap ? m->cap * 2 : 1024;
    Tri *t = realloc(m->tris, sizeof(Tri) * (size_t)cap);
    if (!t) return 0;
    m->tris = t;
    m->cap = cap;
    return 1;
}

static int node_new(Mesh *m) {
    if (m->nodes_count == m->nodes_cap) {
        int cap = m->nodes_cap ? m->nodes_cap * 2 : 256;
        Node *n = realloc(m->nodes, sizeof(Node) * (size_t)cap);
        if (!n) return -1;
        m->nodes = n;
        m->nodes_cap = cap;
    }
    return m->nodes_count++;
}

void mesh_init(Mesh *m, const char *path) {
    memset(m, 0, sizeof *m);
    m->lo = v3(1e30f, 1e30f, 1e30f);
    m->hi = v3(-1e30f, -1e30f, -1e30f);

    const char *slash = strrchr(path, '/');
    snprintf(m->name, sizeof m->name, "%s", slash ? slash + 1 : path);
    char *dot = strchr(m->name, '.');
    if (dot) *dot = '\0';
}

int mesh_add(Mesh *m, Vec3 a, Vec3 b, Vec3 c) {
    if (!tri_reserve(m)) return 0;
    m->tris[m->count].a = a;
    m->tris[m->count].b = b;
    m->tris[m->count].c = c;
    m->count++;

    Vec3 vs[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        if (vs[i].x < m->lo.x) m->lo.x = vs[i].x;
        if (vs[i].y < m->lo.y) m->lo.y = vs[i].y;
        if (vs[i].z < m->lo.z) m->lo.z = vs[i].z;
        if (vs[i].x > m->hi.x) m->hi.x = vs[i].x;
        if (vs[i].y > m->hi.y) m->hi.y = vs[i].y;
        if (vs[i].z > m->hi.z) m->hi.z = vs[i].z;
    }
    return 1;
}

// centroid per triangle, reused by every split
static Vec3 *cent;

static float pick(int tri, int axis) {
    return axis == 0 ? cent[tri].x : axis == 1 ? cent[tri].y : cent[tri].z;
}

static float axis_val(Vec3 c, int axis) {
    return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
}

// hoare split, leaves everything below the pivot in front
static void partition(Mesh *m, int start, int count, int axis, float pivot, int *k) {
    int i = start, j = start + count - 1;
    while (i <= j) {
        while (i <= j && pick(m->order[i], axis) < pivot) i++;
        while (i <= j && pick(m->order[j], axis) > pivot) j--;
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

// split the slice on the longest centroid axis, midpoint pivot
static int build(Mesh *m, int start, int count) {
    int ni = node_new(m);
    if (ni < 0) return -1;

    Vec3 lo = v3(1e30f, 1e30f, 1e30f), hi = v3(-1e30f, -1e30f, -1e30f);
    Vec3 clo = v3(1e30f, 1e30f, 1e30f), chi = v3(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < count; i++) {
        const Tri *t = &m->tris[m->order[start + i]];
        Vec3 vs[3] = {t->a, t->b, t->c};
        Vec3 c = v3_scale(v3_add(v3_add(t->a, t->b), t->c), 1.0f / 3.0f);
        if (c.x < clo.x) clo.x = c.x;
        if (c.y < clo.y) clo.y = c.y;
        if (c.z < clo.z) clo.z = c.z;
        if (c.x > chi.x) chi.x = c.x;
        if (c.y > chi.y) chi.y = c.y;
        if (c.z > chi.z) chi.z = c.z;
        for (int k = 0; k < 3; k++) {
            if (vs[k].x < lo.x) lo.x = vs[k].x;
            if (vs[k].y < lo.y) lo.y = vs[k].y;
            if (vs[k].z < lo.z) lo.z = vs[k].z;
            if (vs[k].x > hi.x) hi.x = vs[k].x;
            if (vs[k].y > hi.y) hi.y = vs[k].y;
            if (vs[k].z > hi.z) hi.z = vs[k].z;
        }
    }
    m->nodes[ni].lo = lo;
    m->nodes[ni].hi = hi;

    if (count <= LEAF) {
        m->nodes[ni].left = -1;
        m->nodes[ni].right = -1;
        m->nodes[ni].start = start;
        m->nodes[ni].count = count;
        return ni;
    }

    float cx = chi.x - clo.x, cy = chi.y - clo.y, cz = chi.z - clo.z;
    int axis = (cx > cy && cx > cz) ? 0 : (cy > cz ? 1 : 2);
    float pivot = (axis_val(clo, axis) + axis_val(chi, axis)) * 0.5f;

    int k = 0;
    partition(m, start, count, axis, pivot, &k);
    if (k <= 0 || k >= count) k = count / 2;

    int l = build(m, start, k);
    int r = build(m, start + k, count - k);
    // recursion grows the node array, so write through the index
    m->nodes[ni].left = l;
    m->nodes[ni].right = r;
    m->nodes[ni].count = 0;
    return ni;
}

int mesh_build(Mesh *m) {
    free(m->order);
    m->order = malloc(sizeof(int) * (size_t)m->count);
    cent = malloc(sizeof(Vec3) * (size_t)m->count);
    if (!m->order || !cent) return 0;
    for (int i = 0; i < m->count; i++) {
        m->order[i] = i;
        cent[i] = v3_scale(v3_add(v3_add(m->tris[i].a, m->tris[i].b), m->tris[i].c), 1.0f / 3.0f);
    }
    m->nodes_count = 0;
    int root = build(m, 0, m->count);
    free(cent);
    cent = NULL;
    return root >= 0;
}

void mesh_free(Mesh *m) {
    free(m->tris);
    free(m->nodes);
    free(m->order);
    memset(m, 0, sizeof *m);
}

// positions first, then fan every face
static int obj_load(Meshes *ms, const char *path) {
    if (ms->count >= MESH_MAX) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    Mesh *m = &ms->items[ms->count];
    mesh_init(m, path);

    int vcap = 1024, vn = 0;
    Vec3 *verts = malloc(sizeof(Vec3) * (size_t)vcap);
    if (!verts) { fclose(f); return -1; }

    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) != 3) continue;
            if (vn == vcap) {
                vcap *= 2;
                Vec3 *nv = realloc(verts, sizeof(Vec3) * (size_t)vcap);
                if (!nv) { free(verts); fclose(f); return -1; }
                verts = nv;
            }
            verts[vn++] = v3(x, y, z);
        } else if (line[0] == 'f' && line[1] == ' ') {
            int idx[FACE_MAX], n = 0;
            char *p = line + 2;
            while (n < FACE_MAX) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p != '-' && (*p < '0' || *p > '9')) break;
                long k = strtol(p, &p, 10);
                if (k == 0) break;
                idx[n++] = (int)k;
                // step over /vt and /vn
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            }
            for (int i = 0; i < n; i++) idx[i] = idx[i] > 0 ? idx[i] - 1 : vn + idx[i];
            for (int i = 1; i + 1 < n; i++) {
                if (idx[0] < 0 || idx[0] >= vn) break;
                if (idx[i] < 0 || idx[i] >= vn) continue;
                if (idx[i + 1] < 0 || idx[i + 1] >= vn) continue;
                if (!mesh_add(m, verts[idx[0]], verts[idx[i]], verts[idx[i + 1]])) {
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

int mesh_load(Meshes *ms, const char *path) {
    const char *dot = strrchr(path, '.');
    if (dot && strcmp(dot, ".glb") == 0) return gltf_load(ms, path);
    return obj_load(ms, path);
}

// one path per line, blank lines and # skipped
int mesh_list(Meshes *ms, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        size_t n = strlen(p);
        while (n && (p[n - 1] == '\n' || p[n - 1] == '\r' || p[n - 1] == ' ')) p[--n] = '\0';
        if (!n || *p == '#') continue;
        mesh_load(ms, p);
    }
    fclose(f);
    return ms->count;
}

int mesh_find(const Meshes *ms, const char *name) {
    for (int i = 0; i < ms->count; i++)
        if (strcmp(ms->items[i].name, name) == 0) return i;
    return -1;
}

// slab test, reports the entry distance
static int slab(Vec3 lo, Vec3 hi, Vec3 o, Vec3 d, float tmax, float *tout) {
    float t0 = 0.0f, t1 = tmax;
    float loa[3] = {lo.x, lo.y, lo.z}, hia[3] = {hi.x, hi.y, hi.z};
    float oa[3] = {o.x, o.y, o.z}, da[3] = {d.x, d.y, d.z};

    for (int i = 0; i < 3; i++) {
        if (__builtin_fabsf(da[i]) < 1e-9f) {
            if (oa[i] < loa[i] || oa[i] > hia[i]) return 0;
            continue;
        }
        float inv = 1.0f / da[i];
        float a = (loa[i] - oa[i]) * inv, b = (hia[i] - oa[i]) * inv;
        if (a > b) { float tmp = a; a = b; b = tmp; }
        if (a > t0) t0 = a;
        if (b < t1) t1 = b;
        if (t0 > t1) return 0;
    }
    *tout = t0;
    return 1;
}

// moller-trumbore, returns the hit distance
static int tri_hit(const Tri *t, Vec3 o, Vec3 d, float tmax, float *tout) {
    Vec3 e1 = v3_sub(t->b, t->a);
    Vec3 e2 = v3_sub(t->c, t->a);
    Vec3 pv = v3_cross(d, e2);
    float det = v3_dot(e1, pv);
    if (__builtin_fabsf(det) < 1e-9f) return 0;

    float inv = 1.0f / det;
    Vec3 tv = v3_sub(o, t->a);
    float u = v3_dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) return 0;

    Vec3 qv = v3_cross(tv, e1);
    float v = v3_dot(d, qv) * inv;
    if (v < 0.0f || u + v > 1.0f) return 0;

    float tt = v3_dot(e2, qv) * inv;
    if (tt <= 0.0f || tt > tmax) return 0;
    *tout = tt;
    return 1;
}

static void walk(const Mesh *m, int ni, Vec3 o, Vec3 d, float *best, int *hit, Vec3 *n) {
    float entry;
    if (!slab(m->nodes[ni].lo, m->nodes[ni].hi, o, d, *best, &entry)) return;

    if (m->nodes[ni].left < 0) {
        for (int i = 0; i < m->nodes[ni].count; i++) {
            const Tri *t = &m->tris[m->order[m->nodes[ni].start + i]];
            float tt;
            if (!tri_hit(t, o, d, *best, &tt)) continue;
            Vec3 e1 = v3_sub(t->b, t->a);
            Vec3 e2 = v3_sub(t->c, t->a);
            *best = tt;
            *n = v3_norm(v3_cross(e1, e2));
            *hit = 1;
        }
        return;
    }

    walk(m, m->nodes[ni].left, o, d, best, hit, n);
    walk(m, m->nodes[ni].right, o, d, best, hit, n);
}

int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n) {
    if (m->nodes_count == 0) return 0;
    float best = 1e30f;
    int hit = 0;
    Vec3 bn = v3(0.0f, 1.0f, 0.0f);
    walk(m, 0, o, d, &best, &hit, &bn);
    if (hit) { *t = best; *n = bn; }
    return hit;
}

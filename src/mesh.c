#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void push_tri(Mesh *m, Vec3 a, Vec3 b, Vec3 c) {
    if (m->count >= TRI_MAX) return;
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
}

// positions first, then fan every face
int mesh_load(Meshes *ms, const char *path) {
    if (ms->count >= MESH_MAX) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    Mesh *m = &ms->items[ms->count];
    memset(m, 0, sizeof *m);
    m->lo = v3(1e30f, 1e30f, 1e30f);
    m->hi = v3(-1e30f, -1e30f, -1e30f);

    const char *slash = strrchr(path, '/');
    snprintf(m->name, sizeof m->name, "%s", slash ? slash + 1 : path);
    char *dot = strchr(m->name, '.');
    if (dot) *dot = '\0';

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
                push_tri(m, verts[idx[0]], verts[idx[i]], verts[idx[i + 1]]);
            }
        }
    }

    free(verts);
    fclose(f);

    if (m->count == 0) return -1;
    return ms->count++;
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

// moller-trumbore
int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n) {
    int hit = 0;
    float best = 0.0f;
    Vec3 bn = v3(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < m->count; i++) {
        Vec3 e1 = v3_sub(m->tris[i].b, m->tris[i].a);
        Vec3 e2 = v3_sub(m->tris[i].c, m->tris[i].a);
        Vec3 pv = v3_cross(d, e2);
        float det = v3_dot(e1, pv);
        if (__builtin_fabsf(det) < 1e-8f) continue;

        float inv = 1.0f / det;
        Vec3 tv = v3_sub(o, m->tris[i].a);
        float u = v3_dot(tv, pv) * inv;
        if (u < 0.0f || u > 1.0f) continue;

        Vec3 qv = v3_cross(tv, e1);
        float v = v3_dot(d, qv) * inv;
        if (v < 0.0f || u + v > 1.0f) continue;

        float tt = v3_dot(e2, qv) * inv;
        if (tt <= 0.0f) continue;
        if (hit && tt >= best) continue;

        best = tt;
        bn = v3_norm(v3_cross(e1, e2));
        hit = 1;
    }

    if (hit) { *t = best; *n = bn; }
    return hit;
}
#include "gltf.h"

#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_JSON 0x4E4F534A
#define CHUNK_BIN  0x004E4942

#define FLOAT 5126
#define UBYTE 5121
#define USHORT 5123
#define UINT 5125

static unsigned rd_u32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static float rd_f32(const unsigned char *p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

// column major, m[col * 4 + row]
static void mat_identity(float *m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat_mul(float *out, const float *a, const float *b) {
    float t[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            t[c * 4 + r] = a[r] * b[c * 4] + b[c * 4 + 1] * a[4 + r] + b[c * 4 + 2] * a[8 + r] + b[c * 4 + 3] * a[12 + r];
    memcpy(out, t, sizeof t);
}

static Vec3 mat_apply(const float *m, Vec3 v) {
    return v3(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
              m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
              m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
}

// gltf nodes carry either a matrix or scale, rotation and translation
static void mat_trs(float *out, JVal *node) {
    float sc[3] = {1.0f, 1.0f, 1.0f}, q[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tr[3] = {0.0f, 0.0f, 0.0f};
    float mx[16];
    if (json_floats(json_get(node, "matrix"), mx, 16)) {
        memcpy(out, mx, sizeof mx);
        return;
    }
    json_floats(json_get(node, "scale"), sc, 3);
    json_floats(json_get(node, "rotation"), q, 4);
    json_floats(json_get(node, "translation"), tr, 3);

    float m[16];
    m[0] = (1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2])) * sc[0];
    m[1] = (2.0f * (q[0] * q[1] + q[2] * q[3])) * sc[0];
    m[2] = (2.0f * (q[0] * q[2] - q[1] * q[3])) * sc[0];
    m[3] = 0.0f;
    m[4] = (2.0f * (q[0] * q[1] - q[2] * q[3])) * sc[1];
    m[5] = (1.0f - 2.0f * (q[0] * q[0] + q[2] * q[2])) * sc[1];
    m[6] = (2.0f * (q[1] * q[2] + q[0] * q[3])) * sc[1];
    m[7] = 0.0f;
    m[8] = (2.0f * (q[0] * q[2] + q[1] * q[3])) * sc[2];
    m[9] = (2.0f * (q[1] * q[2] - q[0] * q[3])) * sc[2];
    m[10] = (1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1])) * sc[2];
    m[11] = 0.0f;
    m[12] = tr[0];
    m[13] = tr[1];
    m[14] = tr[2];
    m[15] = 1.0f;

    mat_identity(out);
    mat_mul(out, out, m);
}

// points at the first component of accessor element i, *stride is the step in bytes
static const unsigned char *accessor(JVal *acc, JVal *g, const unsigned char *bin, size_t bin_size,
                                     int *stride, int *count) {
    if (!acc || acc->kind != JOBJ) return NULL;
    JVal *cnt = json_get(acc, "count");
    if (!cnt) return NULL;
    *count = (int)cnt->num;
    if (*count <= 0) return NULL;

    JVal *comp = json_get(acc, "componentType");
    int ct = comp ? (int)comp->num : 0;
    if (ct == FLOAT) *stride = 12;
    else if (ct == UINT || ct == USHORT) *stride = 4;
    else if (ct == UBYTE) *stride = 1;
    else return NULL;

    JVal *bv = json_get(acc, "bufferView");
    if (!bv) return NULL;
    JVal *view = json_at(json_get(g, "bufferViews"), (int)bv->num);
    if (!view) return NULL;

    JVal *bo = json_get(view, "buffer");
    if (!bo || bo->num != 0) return NULL;

    size_t off = 0;
    JVal *o = json_get(view, "byteOffset");
    if (o) off = (size_t)o->num;
    JVal *a = json_get(acc, "byteOffset");
    if (a) off += (size_t)a->num;

    size_t need = off + (size_t)(*count ? *count - 1 : 0) * (size_t)*stride + (size_t)*stride;
    if (off > bin_size || need > bin_size) return NULL;
    return bin + off;
}

static int index_at(const unsigned char *p, int comp, int i) {
    if (comp == UINT) return (int)rd_u32(p + (size_t)i * 4);
    if (comp == USHORT) return (int)(p[(size_t)i * 2] | (p[(size_t)i * 2 + 1] << 8));
    return p[i];
}

// first node that uses mesh mi, identity when the mesh sits at the root
static void mesh_transform(JVal *g, int mi, float *out) {
    mat_identity(out);
    JVal *nodes = json_get(g, "nodes");
    if (!nodes || nodes->kind != JARR) return;

    for (JVal *n = nodes->child; n; n = n->next) {
        JVal *m = json_get(n, "mesh");
        if (!m || (int)m->num != mi) continue;

        mat_trs(out, n);
        return;
    }
}

int gltf_load(Meshes *ms, const char *path) {
    if (ms->count >= MESH_MAX) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = size > 0 ? malloc((size_t)size) : NULL;
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    if (size < 20 || memcmp(buf, "glTF", 4) != 0) {
        free(buf);
        return -1;
    }

    const unsigned char *js = NULL, *bin = NULL;
    size_t blen = 0;
    for (size_t off = 12; off + 8 <= (size_t)size;) {
        size_t clen = rd_u32(buf + off);
        unsigned ctype = rd_u32(buf + off + 4);
        if (off + 8 + clen > (size_t)size) break;
        if (ctype == CHUNK_JSON) js = buf + off + 8;
        else if (ctype == CHUNK_BIN) { bin = buf + off + 8; blen = clen; }
        off += 8 + clen + (clen & 3);
    }
    if (!js || !bin) {
        free(buf);
        return -1;
    }

    JVal *g = json_parse((char *)js);
    if (!g) {
        free(buf);
        return -1;
    }

    JVal *meshes = json_get(g, "meshes");
    if (!meshes || meshes->kind != JARR || meshes->count == 0) {
        json_free();
        free(buf);
        return -1;
    }

    Mesh *m = &ms->items[ms->count];
    mesh_init(m, path);
    JVal *accessors = json_get(g, "accessors");

    for (int mi = 0; mi < meshes->count; mi++) {
        JVal *prims = json_get(json_at(meshes, mi), "primitives");
        if (!prims || prims->kind != JARR) continue;

        float xf[16];
        mesh_transform(g, mi, xf);

        for (JVal *p = prims->child; p; p = p->next) {
            JVal *mode = json_get(p, "mode");
            if (mode && (int)mode->num != 4) continue;   // triangles only

            JVal *attrs = json_get(p, "attributes");
            JVal *posj = json_get(attrs, "POSITION");
            JVal *idxj = json_get(p, "indices");
            if (!accessors || !posj || !idxj) continue;

            int stride = 12, vn = 0;
            const unsigned char *pos = accessor(json_at(accessors, (int)posj->num), g, bin, blen, &stride, &vn);
            if (!pos || stride != 12 || vn == 0) continue;

            int istride = 4, idxn = 0;
            const unsigned char *idx = accessor(json_at(accessors, (int)idxj->num), g, bin, blen, &istride, &idxn);
            if (!idx || idxn < 3) continue;
            if (istride != 1 && istride != 2 && istride != 4) continue;

            Vec3 *verts = malloc(sizeof(Vec3) * (size_t)vn);
            if (!verts) continue;
            for (int i = 0; i < vn; i++) {
                const unsigned char *v = pos + (size_t)i * 12;
                verts[i] = mat_apply(xf, v3(rd_f32(v), rd_f32(v + 4), rd_f32(v + 8)));
            }

            int comp = istride == 4 ? UINT : (istride == 2 ? USHORT : UBYTE);
            for (int i = 0; i + 2 < idxn; i += 3) {
                int a = index_at(idx, comp, i), b = index_at(idx, comp, i + 1), c = index_at(idx, comp, i + 2);
                if (a < 0 || a >= vn || b < 0 || b >= vn || c < 0 || c >= vn) continue;
                if (!mesh_add(m, verts[a], verts[b], verts[c])) {
                    free(verts);
                    goto done;
                }
            }
            free(verts);
        }
    }

done:
    json_free();
    free(buf);

    if (m->count == 0 || !mesh_build(m)) {
        mesh_free(m);
        return -1;
    }
    return ms->count++;
}

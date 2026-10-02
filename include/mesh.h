#ifndef MESH_H
#define MESH_H

#include "vec3.h"

#define MESH_MAX 16
#define TRI_MAX  8192
#define FACE_MAX 32

typedef struct {
    Vec3 a, b, c;
} Tri;

typedef struct {
    char name[64];
    Tri tris[TRI_MAX];
    int count;
    Vec3 lo, hi;
} Mesh;

typedef struct {
    Mesh items[MESH_MAX];
    int count;
} Meshes;

int mesh_load(Meshes *ms, const char *path);
int mesh_list(Meshes *ms, const char *path);
int mesh_find(const Meshes *ms, const char *name);
int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n);

#endif
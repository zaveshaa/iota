#ifndef MESH_H
#define MESH_H

#include "vec3.h"

#define MESH_MAX 16
#define FACE_MAX 32
#define MESH_LEAF 4

typedef struct {
    Vec3 a;
    Vec3 b;
    Vec3 c;
} Tri;

typedef struct {
    Vec3 lo;
    Vec3 hi;
    int left;
    int right;
    int start;
    int count;
} MeshNode;

typedef struct {
    char name[64];
    Tri *tris;
    int count;
    int cap;
    Vec3 lo;
    Vec3 hi;
    MeshNode *nodes;
    int nodes_count;
    int nodes_cap;
    int *order;
} Mesh;

typedef struct {
    Mesh items[MESH_MAX];
    int count;
} Meshes;

void mesh_init(Mesh *m, const char *path);
int mesh_add(Mesh *m, Vec3 a, Vec3 b, Vec3 c);
int mesh_build(Mesh *m);
void mesh_free(Mesh *m);

int mesh_load(Meshes *ms, const char *path);
int mesh_list(Meshes *ms, const char *path);
int mesh_find(const Meshes *ms, const char *name);
int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n);

#endif

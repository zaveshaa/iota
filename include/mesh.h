#ifndef MESH_H
#define MESH_H

#include "vec3.h"

#define MESH_MAX 16
#define FACE_MAX 32
#define LEAF     4

typedef struct {
    Vec3 a, b, c;
} Tri;

typedef struct {
    Vec3 lo, hi;
    int left;   // -1 on a leaf
    int right;
    int start;  // slice of order
    int count;
} Node;

typedef struct {
    char name[64];
    Tri *tris;
    int count, cap;
    Vec3 lo, hi;
    Node *nodes;
    int nodes_count, nodes_cap;
    int *order;
} Mesh;

typedef struct {
    Mesh items[MESH_MAX];
    int count;
} Meshes;

int mesh_load(Meshes *ms, const char *path);
int mesh_list(Meshes *ms, const char *path);
int mesh_find(const Meshes *ms, const char *name);
int mesh_tri_hit(const Mesh *m, Vec3 o, Vec3 d, float *t, Vec3 *n);

void mesh_init(Mesh *m, const char *path);
int mesh_add(Mesh *m, Vec3 a, Vec3 b, Vec3 c);
int mesh_build(Mesh *m);
void mesh_free(Mesh *m);

#endif

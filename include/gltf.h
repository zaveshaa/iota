#ifndef GLTF_H
#define GLTF_H

#include "mesh.h"

// loads a binary gltf into ms, returns the mesh index or -1
int gltf_load(Meshes *ms, const char *path);

#endif

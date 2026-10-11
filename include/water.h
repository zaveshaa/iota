#ifndef WATER_H
#define WATER_H

#include "vec3.h"

typedef struct {
    float x0;
    float z0;
    float x1;
    float z1;
    float base_h;
    float spacing;
    int nx;
    int nz;
    float t;
    float *cur;
    float *prev;
    float *next;
} Water;

void water_open(Water *w, Vec3 lo, Vec3 hi, float base_h, float spacing);
void water_close(Water *w);
void water_step(Water *w, float dt);
int water_body(Water *w, Vec3 feet, float radius, Vec3 vel, float dt);
float water_surface(const Water *w, float x, float z);
int water_cast(const Water *w, Vec3 origin, Vec3 dir, float tmax,
               float *t_out, Vec3 *n_out, float *a_out);

#endif
#ifndef WATER_H
#define WATER_H

#include "vec3.h"

#define WATER_STEAM_MAX 64

typedef struct {
    Vec3 p;
    Vec3 v;
    float life;
    float span;
} Steam;

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
    int spout_on;
    float spout_x;
    float spout_y;
    float spout_z;
    float spout_rate;
    float spawn_acc;
    unsigned rng;
    int steam_n;
    Steam steam[WATER_STEAM_MAX];
} Water;

void water_open(Water *w, Vec3 lo, Vec3 hi, float base_h, float spacing);
void water_close(Water *w);
void water_step(Water *w, float dt);
void water_spout(Water *w, Vec3 top, float rate);
int water_body(Water *w, Vec3 feet, float radius, Vec3 vel, float dt);
float water_surface(const Water *w, float x, float z);
int water_cast(const Water *w, Vec3 origin, Vec3 dir, float tmax,
               float *t_out, Vec3 *n_out, float *a_out);

#endif

#ifndef WATER_H
#define WATER_H

#include "vec3.h"

#define WATER_STEAM_MAX 64
#define WATER_SPOUT_MAX 6

typedef struct {
    Vec3 p;
    Vec3 v;
    float life;
    float span;
    float radius;
    unsigned seed;
} Steam;

// a pipe or spring feeding the pool: where its mouth sits above the water and
// how hard it pours, plus the steam it has saved up to release
typedef struct {
    Vec3 top;
    float rate;
    float spawn_acc;
} Spout;

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
    Spout spouts[WATER_SPOUT_MAX];
    int spout_n;
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

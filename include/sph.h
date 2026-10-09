#ifndef SPH_H
#define SPH_H

#include "vec3.h"

#define SPH_MAX 2048
#define SPH_GRID 512
#define SPH_SMOOTH 0.5f
#define SPH_DT (1.0f / 120.0f)
#define SPH_REST 150.0f
#define SPH_PRESS 38.0f
#define SPH_VISC 0.25f
#define SPH_G 12.0f
#define SPH_PT_R 0.09f

typedef struct {
    Vec3 pos;
    Vec3 vel;
    float dens;
} SphPt;

typedef struct {
    SphPt pt[SPH_MAX];
    int count;
    float lo_x;
    float lo_y;
    float lo_z;
    float hi_x;
    float hi_y;
    float hi_z;
    int gx;
    int gy;
    int gz;
    int head[SPH_GRID];
    int next[SPH_MAX];
    float kpoly;
    float kspiky;
    float kvisc;
    float wall_h;
    float lip_z;
} Sph;

void sph_init(Sph *sp, Vec3 lo, Vec3 hi);
void sph_fill(Sph *sp, Vec3 lo, Vec3 hi, float spacing);
void sph_walls(Sph *sp, float height, float lip);
int sph_emit(Sph *sp, Vec3 at, Vec3 vel, float spread);
void sph_step(Sph *sp, float dt);
int sph_body(Sph *sp, Vec3 feet, float radius, float height, Vec3 vel);
int sph_cast(const Sph *sp, Vec3 origin, Vec3 dir, float tmax, float *t_out,
             Vec3 *n_out, float *d_out);

#endif

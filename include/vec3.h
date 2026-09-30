#ifndef VEC3_H
#define VEC3_H

typedef struct { float x, y, z; } Vec3;

static inline Vec3 v3(float x, float y, float z) { return (Vec3){x, y, z}; }

static inline Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static inline Vec3 v3_cross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline Vec3 v3_norm(Vec3 a) {
    float len = __builtin_sqrtf(v3_dot(a, a));
    return len > 0.0f ? v3_scale(a, 1.0f / len) : a;
}

#endif

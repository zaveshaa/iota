#include "level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEVEL_MAX_TOK 24

static int split(char *line, char **tok, int max)
{
    char *p = line;
    int n = 0;

    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            break;
        }
        if (n >= max) {
            break;
        }
        tok[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' &&
               *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return n;
}

static float tf(const char *s)
{
    return strtof(s, NULL);
}

static unsigned tu(const char *s)
{
    return (unsigned)strtoul(s, NULL, 10);
}

static unsigned tx(const char *s)
{
    return (unsigned)strtoul(s, NULL, 16);
}

static const char *kind_name(ObjKind k)
{
    switch (k) {
    case OBJ_SPHERE:
        return "sphere";
    case OBJ_PLANE:
        return "plane";
    case OBJ_MESH:
        return "mesh";
    default:
        return "box";
    }
}

static ObjKind kind_of(const char *s)
{
    if (strcmp(s, "sphere") == 0) {
        return OBJ_SPHERE;
    }
    if (strcmp(s, "plane") == 0) {
        return OBJ_PLANE;
    }
    if (strcmp(s, "mesh") == 0) {
        return OBJ_MESH;
    }
    return OBJ_BOX;
}

int level_save(const Scene *s, const char *path)
{
    FILE *f = fopen(path, "w");
    int i;

    if (f == NULL) {
        return -1;
    }
    (void)fprintf(f, "iota 1\n");
    if (s->have_spawn) {
        (void)fprintf(f, "spawn %g %g %g\n", (double)s->spawn.x,
                      (double)s->spawn.y, (double)s->spawn.z);
    }
    (void)fprintf(f, "ambient %06x\n", s->ambient & 0xffffffu);
    (void)fprintf(f, "fog %06x %g %g\n", s->fog & 0xffffffu,
                  (double)s->fog_start, (double)s->fog_range);
    (void)fprintf(f, "sky %06x %06x", s->sky_top & 0xffffffu,
                  s->sky_horizon & 0xffffffu);
    if (s->have_moon) {
        (void)fprintf(f, " %g %g %g %06x %g", (double)s->sky_moon_dir.x,
                      (double)s->sky_moon_dir.y, (double)s->sky_moon_dir.z,
                      s->sky_moon & 0xffffffu, (double)s->sky_moon_size);
    }
    (void)fprintf(f, "\n");
    if (s->have_water) {
        (void)fprintf(f, "water %g %g %g %g %g %g", (double)s->water.lo.x,
                      (double)s->water.lo.z, (double)s->water.hi.x,
                      (double)s->water.hi.z, (double)s->water.level,
                      (double)s->water.spacing);
        if (s->water.heat > 0.0f) {
            (void)fprintf(f, " %g", (double)s->water.heat);
        }
        (void)fprintf(f, "\n");
    }
    for (i = 0; i < s->water.spout_count; i++) {
        const SpoutSpec *sp = &s->water.spouts[i];

        (void)fprintf(f, "spout %g %g %g %g\n", (double)sp->top.x,
                      (double)sp->top.y, (double)sp->top.z, (double)sp->rate);
    }
    for (i = 0; i < s->obj_count; i++) {
        const Obj *o = &s->objs[i];

        (void)fprintf(f,
                      "obj %s %g %g %g %g %g %g %g %g %g %06x %u %d %g\n",
                      kind_name(o->kind), (double)o->pos.x, (double)o->pos.y,
                      (double)o->pos.z, (double)o->half.x, (double)o->half.y,
                      (double)o->half.z, (double)o->axis.x, (double)o->axis.y,
                      (double)o->axis.z, o->ink & 0xffffffu, o->flags, o->mesh,
                      (double)o->yaw);
    }
    for (i = 0; i < s->light_count; i++) {
        const Light *l = &s->lights[i];

        (void)fprintf(f, "light %s %g %g %g %g %g %g %g\n",
                      l->kind == LIGHT_SUN ? "sun" : "point",
                      (double)l->pos.x, (double)l->pos.y, (double)l->pos.z,
                      (double)l->color.x, (double)l->color.y,
                      (double)l->color.z, (double)l->intensity);
    }
    if (fclose(f) != 0) {
        return -1;
    }
    return 0;
}

int level_load(Scene *s, const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];

    if (f == NULL) {
        return -1;
    }
    scene_clear(s);
    while (fgets(line, sizeof line, f) != NULL) {
        char *tok[LEVEL_MAX_TOK];
        int n = split(line, tok, LEVEL_MAX_TOK);

        if (n == 0) {
            continue;
        }
        if (strcmp(tok[0], "iota") == 0) {
            continue;
        }
        if (strcmp(tok[0], "spawn") == 0 && n >= 4) {
            s->spawn = v3(tf(tok[1]), tf(tok[2]), tf(tok[3]));
            s->have_spawn = 1;
        } else if (strcmp(tok[0], "ambient") == 0 && n >= 2) {
            s->ambient = tx(tok[1]);
        } else if (strcmp(tok[0], "fog") == 0 && n >= 4) {
            s->fog = tx(tok[1]);
            s->fog_start = tf(tok[2]);
            s->fog_range = tf(tok[3]);
        } else if (strcmp(tok[0], "sky") == 0 && n >= 3) {
            s->sky_top = tx(tok[1]);
            s->sky_horizon = tx(tok[2]);
            if (n >= 8) {
                s->sky_moon_dir = v3(tf(tok[3]), tf(tok[4]), tf(tok[5]));
                s->sky_moon = tx(tok[6]);
                s->sky_moon_size = tf(tok[7]);
                s->have_moon = 1;
            }
        } else if (strcmp(tok[0], "water") == 0 && n >= 7) {
            s->water.lo = v3(tf(tok[1]), 0.0f, tf(tok[2]));
            s->water.hi = v3(tf(tok[3]), 0.0f, tf(tok[4]));
            s->water.level = tf(tok[5]);
            s->water.spacing = tf(tok[6]);
            s->water.heat = n >= 8 ? tf(tok[7]) : 0.0f;
            s->have_water = 1;
        } else if (strcmp(tok[0], "spout") == 0 && n >= 5) {
            if (s->water.spout_count < SCENE_MAX_SPOUTS) {
                SpoutSpec *sp = &s->water.spouts[s->water.spout_count];

                s->water.spout_count++;
                sp->top = v3(tf(tok[1]), tf(tok[2]), tf(tok[3]));
                sp->rate = tf(tok[4]);
            }
        } else if (strcmp(tok[0], "obj") == 0 && n >= 13) {
            Obj *o = scene_add_obj(s);

            if (o == NULL) {
                break;
            }
            o->kind = kind_of(tok[1]);
            o->pos = v3(tf(tok[2]), tf(tok[3]), tf(tok[4]));
            o->half = v3(tf(tok[5]), tf(tok[6]), tf(tok[7]));
            o->axis = v3(tf(tok[8]), tf(tok[9]), tf(tok[10]));
            o->ink = tx(tok[11]);
            o->flags = tu(tok[12]);
            o->mesh = n >= 14 ? (int)tu(tok[13]) : -1;
            o->yaw = n >= 15 ? tf(tok[14]) : 0.0f;
        } else if (strcmp(tok[0], "light") == 0 && n >= 9) {
            Light *l = scene_add_light(s);

            if (l == NULL) {
                break;
            }
            l->kind = strcmp(tok[1], "sun") == 0 ? LIGHT_SUN : LIGHT_POINT;
            l->pos = v3(tf(tok[2]), tf(tok[3]), tf(tok[4]));
            l->color = v3(tf(tok[5]), tf(tok[6]), tf(tok[7]));
            l->intensity = tf(tok[8]);
        }
    }
    (void)fclose(f);
    return 0;
}

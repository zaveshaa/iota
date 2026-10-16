#include "level.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

static void check(int ok, const char *name)
{
    g_checks++;
    if (ok) {
        (void)printf("ok   %s\n", name);
        return;
    }
    g_failures++;
    (void)printf("FAIL %s\n", name);
}

static void round_trip(void)
{
    Scene a;
    Scene b;
    Obj *o;
    Light *l;

    scene_clear(&a);
    a.have_spawn = 1;
    a.spawn = v3(1.0f, 0.0f, -3.0f);
    o = scene_add_obj(&a);
    o->kind = OBJ_BOX;
    o->pos = v3(2.0f, -0.5f, 4.0f);
    o->half = v3(3.0f, 0.5f, 3.0f);
    o->ink = RGB(96, 100, 116);
    o->flags = OBJ_TILE;
    o = scene_add_obj(&a);
    o->kind = OBJ_SPHERE;
    o->pos = v3(-1.0f, 0.7f, 5.0f);
    o->half = v3(0.7f, 0.7f, 0.7f);
    o->ink = RGB(220, 220, 220);
    o->flags = OBJ_MIRROR;
    o->yaw = 0.785398f;
    l = scene_add_light(&a);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 4.0f, 4.0f);
    l->color = v3(1.0f, 0.97f, 0.92f);
    l->intensity = 4.0f;
    a.have_water = 1;
    a.water.lo = v3(-2.0f, 0.0f, -1.0f);
    a.water.hi = v3(2.0f, 0.0f, 1.0f);
    a.water.level = -0.8f;
    a.water.spacing = 0.5f;

    check(level_save(&a, "leveltest.lvl") == 0, "a scene writes to a file");
    check(level_load(&b, "leveltest.lvl") == 0, "a scene reads back");
    check(b.have_spawn && b.spawn.x == 1.0f && b.spawn.z == -3.0f,
          "the spawn comes back");
    check(b.obj_count == 2, "every object comes back");
    check(b.light_count == 1, "every light comes back");
    check(b.objs[0].kind == OBJ_BOX && b.objs[0].flags == OBJ_TILE &&
              b.objs[0].ink == RGB(96, 100, 116),
          "a tiled box keeps its flag and colour");
    check(b.objs[1].kind == OBJ_SPHERE && b.objs[1].flags == OBJ_MIRROR &&
              b.objs[1].yaw == 0.785398f,
          "a mirrored ball keeps its flag and turn");
    check(b.lights[0].kind == LIGHT_POINT && b.lights[0].intensity == 4.0f,
          "a lamp keeps its kind and power");
    check(b.have_water && b.water.lo.x == -2.0f && b.water.hi.z == 1.0f &&
              b.water.level == -0.8f,
          "a pool keeps its breadth and level");
    (void)remove("leveltest.lvl");
}

int main(void)
{
    round_trip();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

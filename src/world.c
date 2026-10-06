#include "world.h"

#include <string.h>

void scene_clear(Scene *s)
{
    memset(s, 0, sizeof *s);
    s->ambient = RGB(46, 52, 68);
    s->fog = RGB(6, 8, 14);
    s->fog_start = 8.0f;
    s->fog_range = 40.0f;
}

Obj *scene_add_obj(Scene *s)
{
    Obj *o;

    if (s->obj_count >= SCENE_MAX_OBJS) {
        return NULL;
    }
    o = &s->objs[s->obj_count];
    s->obj_count++;
    memset(o, 0, sizeof *o);
    o->kind = OBJ_BOX;
    o->ink = RGB(200, 200, 200);
    o->axis = v3(0.0f, 1.0f, 0.0f);
    return o;
}

Light *scene_add_light(Scene *s)
{
    Light *l;

    if (s->light_count >= SCENE_MAX_LIGHTS) {
        return NULL;
    }
    l = &s->lights[s->light_count];
    s->light_count++;
    memset(l, 0, sizeof *l);
    l->kind = LIGHT_POINT;
    l->color = v3(1.0f, 1.0f, 1.0f);
    l->intensity = 1.0f;
    return l;
}

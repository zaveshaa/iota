#include "scene.h"

void mirror_scene(Scene *s, Camera *cam)
{
    Obj *o;
    Light *l;

    scene_clear(s);

    o = scene_add_obj(s);
    o->kind = OBJ_PLANE;
    o->pos = v3(0.0f, 0.0f, 0.0f);
    o->axis = v3(0.0f, 1.0f, 0.0f);
    o->ink = RGB(96, 100, 116);

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 1.6f, 8.5f);
    o->half = v3(6.0f, 1.6f, 0.15f);
    o->ink = RGB(76, 84, 104);

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(0.0f, 1.7f, 8.30f);
    o->half = v3(3.2f, 1.5f, 0.06f);
    o->ink = RGB(210, 214, 224);
    o->flags = OBJ_MIRROR;

    o = scene_add_obj(s);
    o->kind = OBJ_SPHERE;
    o->pos = v3(1.5f, 0.7f, 2.8f);
    o->half = v3(0.7f, 0.7f, 0.7f);
    o->ink = RGB(220, 220, 220);
    o->flags = OBJ_MIRROR;

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(-2.4f, 1.0f, 4.0f);
    o->half = v3(0.5f, 1.0f, 0.5f);
    o->ink = RGB(224, 144, 64);

    o = scene_add_obj(s);
    o->kind = OBJ_BOX;
    o->pos = v3(2.4f, 1.0f, 5.5f);
    o->half = v3(0.5f, 1.0f, 0.5f);
    o->ink = RGB(70, 180, 220);

    l = scene_add_light(s);
    l->kind = LIGHT_POINT;
    l->pos = v3(0.0f, 3.5f, 3.0f);
    l->color = v3(1.0f, 0.97f, 0.92f);
    l->intensity = 4.0f;

    l = scene_add_light(s);
    l->kind = LIGHT_POINT;
    l->pos = v3(-3.0f, 3.5f, 1.0f);
    l->color = v3(1.0f, 0.9f, 0.8f);
    l->intensity = 2.5f;

    cam->pos = v3(0.0f, 1.7f, -1.4f);
    cam->yaw = 0.0f;
    cam->pitch = -0.02f;
    cam->fov = 1.05f;
}

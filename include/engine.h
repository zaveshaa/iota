#ifndef ENGINE_H
#define ENGINE_H

#include "render.h"
#include "term.h"

#define IOTA_VERSION_MAJOR 0
#define IOTA_VERSION_MINOR 1
#define IOTA_VERSION_PATCH 0
#define IOTA_VERSION "0.1.0"

typedef struct Engine Engine;

typedef struct {
    void (*poll)(Engine *e);
    void (*simulate)(Engine *e, float dt);
    void (*draw)(Engine *e);
} EngineHooks;

Engine *engine_open(const EngineHooks *hooks, void *user);
void engine_run(Engine *e);
void engine_close(Engine *e);
void engine_quit(Engine *e);
Render *engine_render(Engine *e);
void *engine_user(Engine *e);
double engine_time(const Engine *e);
double engine_draw_ms(const Engine *e);
double engine_alpha(const Engine *e);
int engine_ss(const Engine *e);

#endif

#include "engine.h"

#include <stdlib.h>
#include <time.h>

#define STEP (1.0 / 60.0)
#define FRAME_TIME 0.016

struct Engine {
    Render *render;
    EngineHooks hooks;
    void *user;
    double t0;
    double acc;
    double draw_ms;
    int quit;
    int ss;
};

static double now_sec(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

Engine *engine_open(const EngineHooks *hooks, void *user)
{
    Engine *e = calloc(1, sizeof *e);

    if (e == NULL) {
        return NULL;
    }
    if (term_open() == NULL) {
        free(e);
        return NULL;
    }
    e->render = render_open(term_cols(), term_rows());
    if (e->render == NULL) {
        term_close();
        free(e);
        return NULL;
    }
    if (hooks != NULL) {
        e->hooks = *hooks;
    }
    e->user = user;
    e->ss = 1;
    e->t0 = now_sec();
    return e;
}

void engine_close(Engine *e)
{
    if (e == NULL) {
        return;
    }
    render_close(e->render);
    term_close();
    free(e);
}

void engine_quit(Engine *e)
{
    e->quit = 1;
}

Render *engine_render(Engine *e)
{
    return e->render;
}

void *engine_user(Engine *e)
{
    return e->user;
}

double engine_time(const Engine *e)
{
    return now_sec() - e->t0;
}

double engine_draw_ms(const Engine *e)
{
    return e->draw_ms;
}

int engine_ss(const Engine *e)
{
    return e->ss;
}

static void engine_resize(Engine *e)
{
    int cols = term_cols();
    int rows = term_rows();

    if (cols != render_cols(e->render) || rows != render_rows(e->render)) {
        render_resize(e->render, cols, rows);
    }
}

void engine_run(Engine *e)
{
    double last = now_sec();

    while (!e->quit) {
        double iter_start = now_sec();
        double t;
        double dt;
        double d0;
        int steps = 0;

        if (e->hooks.poll != NULL) {
            e->hooks.poll(e);
        }
        if (e->quit) {
            break;
        }
        engine_resize(e);

        t = now_sec();
        dt = t - last;
        last = t;
        if (dt > 0.25) {
            dt = 0.25;
        }
        e->acc += dt;
        if (e->hooks.simulate != NULL) {
            while (e->acc >= STEP && steps < 5) {
                e->hooks.simulate(e, (float)STEP);
                e->acc -= STEP;
                steps++;
            }
            if (steps == 5) {
                e->acc = 0.0;
            }
        }

        d0 = now_sec();
        render_clear(e->render, 0x000000u);
        if (e->hooks.draw != NULL) {
            e->hooks.draw(e);
        }
        render_present(e->render);
        e->draw_ms = (now_sec() - d0) * 1000.0;

        if (e->draw_ms < 8.0 && e->ss == 1) {
            e->ss = 2;
        } else if (e->draw_ms > 11.5 && e->ss == 2) {
            e->ss = 1;
        }

        {
            double iter = now_sec() - iter_start;

            if (iter < FRAME_TIME) {
                struct timespec ts;
                double left = FRAME_TIME - iter;

                ts.tv_sec = 0;
                ts.tv_nsec = (long)(left * 1000000000.0);
                (void)nanosleep(&ts, NULL);
            }
        }
    }
}

#include "render.h"

#include <stdio.h>
#include <stdlib.h>


#define RAMP_LEN 10
const char RENDER_RAMP[] = " .:-=+*#%@";

static char *buf;
static float *depth;
static char *out;
static int cols, rows;

int render_shade(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (int)(t * (RAMP_LEN - 1) + 0.5f);
}

void render_init(int c, int r) {
    cols = c;
    rows = r;
    buf = malloc((size_t)cols * (size_t)rows);
    depth = malloc((size_t)cols * (size_t)rows * sizeof(float));
    out = malloc((size_t)cols * (size_t)rows + (size_t)rows * 2);
    render_clear();
}

void render_resize(int c, int r) {
    render_free();
    render_init(c, r);
}

void render_free(void) {
    free(buf);
    free(depth);
    free(out);
    buf = NULL;
    depth = NULL;
    out = NULL;
    cols = rows = 0;
}

void render_clear(void) {
    for (int i = 0; i < cols * rows; i++) {
        buf[i] = ' ';
        depth[i] = RENDER_FAR;
    }
}

void render_put(int col, int row, char ch, float z) {
    if (col < 0 || col >= cols || row < 0 || row >= rows) return;
    int i = row * cols + col;
    // closer wins, RENDER_FAR never does
    if (z < depth[i]) {
        buf[i] = ch;
        depth[i] = z;
    }
}

void render_text(int col, int row, const char *s) {
    if (row < 0 || row >= rows) return;
    // negative depth, so it always lands on top
    for (int i = 0; s[i]; i++) render_put(col + i, row, s[i], -1.0f);
}

void render_present(void) {
    size_t n = 0;
    for (int row = 0; row < rows; row++) {
        if (row) out[n++] = '\n';
        for (int col = 0; col < cols; col++) out[n++] = buf[row * cols + col];
    }
    out[n++] = 0x1B; out[n++] = '['; out[n++] = 'H';
    fwrite(out, 1, n, stdout);
    fflush(stdout);
}

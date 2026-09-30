#define _POSIX_C_SOURCE 200809L

#include "render.h"

#include <stdio.h>
#include <stdlib.h>


#define RAMP_LEN 10
const char RENDER_RAMP[] = " .:-=+*#%@";

static char *buf;
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
    out = malloc((size_t)cols * (size_t)rows + (size_t)rows * 2);
    render_clear();
}

void render_resize(int c, int r) {
    free(buf);
    free(out);
    render_init(c, r);
}

void render_clear(void) {
    for (int i = 0; i < cols * rows; i++) buf[i] = ' ';
}

void render_put(int col, int row, char ch, float z) {
    (void)z;
    if (col < 0 || col >= cols || row < 0 || row >= rows) return;
    buf[row * cols + col] = ch;
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

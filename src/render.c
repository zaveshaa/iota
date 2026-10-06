#include "render.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct Cell {
    unsigned char ch;
    unsigned ink;
};

struct Render {
    int cols;
    int rows;
    size_t n;
    struct Cell *cells;
};

static int render_grid(Render *r, int cols, int rows)
{
    size_t n = (size_t)cols * (size_t)rows;
    struct Cell *cells = malloc(n * sizeof *cells);

    if (cells == NULL) {
        return -1;
    }
    free(r->cells);
    r->cells = cells;
    r->cols = cols;
    r->rows = rows;
    r->n = n;
    render_clear(r, 0x000000u);
    return 0;
}

Render *render_open(int cols, int rows)
{
    Render *r = calloc(1, sizeof *r);

    if (r == NULL) {
        return NULL;
    }
    if (render_grid(r, cols, rows) != 0) {
        free(r);
        return NULL;
    }
    return r;
}

void render_close(Render *r)
{
    if (r == NULL) {
        return;
    }
    free(r->cells);
    free(r);
}

void render_resize(Render *r, int cols, int rows)
{
    if (cols == r->cols && rows == r->rows) {
        return;
    }
    (void)render_grid(r, cols, rows);
}

int render_cols(const Render *r)
{
    return r->cols;
}

int render_rows(const Render *r)
{
    return r->rows;
}

void render_clear(Render *r, unsigned ink)
{
    size_t i;

    for (i = 0; i < r->n; i++) {
        r->cells[i].ch = (unsigned char)' ';
        r->cells[i].ink = ink;
    }
}

void render_put(Render *r, int x, int y, unsigned char ch, unsigned ink)
{
    size_t idx;

    if (x < 0 || y < 0 || x >= r->cols || y >= r->rows) {
        return;
    }
    idx = (size_t)y * (size_t)r->cols + (size_t)x;
    r->cells[idx].ch = ch;
    r->cells[idx].ink = ink;
}

void render_text(Render *r, int x, int y, const char *s, unsigned ink)
{
    int cx = x;

    while (*s != '\0' && y >= 0 && y < r->rows) {
        if (cx >= 0 && cx < r->cols) {
            render_put(r, cx, y, (unsigned char)*s, ink);
        }
        cx++;
        s++;
    }
}

unsigned char render_char(const Render *r, int x, int y)
{
    size_t idx;

    if (x < 0 || y < 0 || x >= r->cols || y >= r->rows) {
        return (unsigned char)'\0';
    }
    idx = (size_t)y * (size_t)r->cols + (size_t)x;
    return r->cells[idx].ch;
}

unsigned render_ink(const Render *r, int x, int y)
{
    size_t idx;

    if (x < 0 || y < 0 || x >= r->cols || y >= r->rows) {
        return 0u;
    }
    idx = (size_t)y * (size_t)r->cols + (size_t)x;
    return r->cells[idx].ink;
}

static char *fmt_add(char *p, char *end, const char *fmt, ...)
{
    va_list ap;
    int k;
    size_t rem = (size_t)(end - p);

    if (rem == 0) {
        return p;
    }
    va_start(ap, fmt);
    k = vsnprintf(p, rem, fmt, ap);
    va_end(ap);
    if (k < 0 || (size_t)k >= rem) {
        return end;
    }
    return p + k;
}

void render_present(Render *r)
{
    size_t bound = r->n * 20u + (size_t)r->rows * 12u + 32u;
    char *buf = malloc(bound);
    char *p;
    char *end;
    unsigned cur = 0x01000000u;
    int x;
    int y;

    if (buf == NULL) {
        return;
    }
    p = buf;
    end = buf + bound;
    for (y = 0; y < r->rows; y++) {
        p = fmt_add(p, end, "\x1b[%d;1H", y + 1);
        for (x = 0; x < r->cols; x++) {
            size_t idx = (size_t)y * (size_t)r->cols + (size_t)x;
            struct Cell c = r->cells[idx];

            if (c.ink != cur) {
                p = fmt_add(p, end, "\x1b[38;2;%u;%u;%um",
                            (c.ink >> 16) & 0xffu, (c.ink >> 8) & 0xffu,
                            c.ink & 0xffu);
                cur = c.ink;
            }
            if (p < end) {
                *p = (char)c.ch;
                p++;
            }
        }
    }
    if (p > buf) {
        size_t total = (size_t)(p - buf);
        size_t off = 0;

        while (off < total) {
            ssize_t wr = write(STDOUT_FILENO, buf + off, total - off);

            if (wr < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (wr == 0) {
                break;
            }
            off += (size_t)wr;
        }
    }
    free(buf);
}

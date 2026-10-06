#ifndef RENDER_H
#define RENDER_H

typedef struct Render Render;

Render *render_open(int cols, int rows);
void render_close(Render *r);
void render_resize(Render *r, int cols, int rows);
int render_cols(const Render *r);
int render_rows(const Render *r);
void render_clear(Render *r, unsigned ink);
void render_put(Render *r, int x, int y, unsigned char ch, unsigned ink);
void render_text(Render *r, int x, int y, const char *s, unsigned ink);
void render_present(Render *r);
unsigned char render_char(const Render *r, int x, int y);
unsigned render_ink(const Render *r, int x, int y);

#endif

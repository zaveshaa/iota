#ifndef RENDER_H
#define RENDER_H

#define RENDER_SKY '.'
#define RENDER_FAR 1e30f

extern const char RENDER_RAMP[];
int  render_shade(float t);

void render_init(int cols, int rows);
void render_resize(int cols, int rows);
void render_free(void);
void render_clear(void);
void render_put(int col, int row, char ch, float z);
void render_text(int col, int row, const char *s);
void render_present(void);

#endif

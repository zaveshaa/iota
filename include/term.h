#ifndef TERM_H
#define TERM_H

void term_init(void);
void term_close(void);
int  term_cols(void);
int  term_rows(void);
int  term_dirty(void);
void term_clean(void);

typedef enum {
    KEY_NONE,
    KEY_W, KEY_A, KEY_S, KEY_D,
    KEY_TURN_L, KEY_TURN_R, KEY_TURN_U, KEY_TURN_D,
    KEY_PLACE, KEY_DEL,
    KEY_BOX, KEY_SPHERE, KEY_MESH,
    KEY_PREV, KEY_NEXT,
    KEY_SAVE, KEY_LOAD,
    KEY_TAB,
    KEY_TURN_OBJ_L, KEY_TURN_OBJ_R, KEY_SCALE,
    KEY_ZOOM_IN, KEY_ZOOM_OUT,
    KEY_ESC,
    KEY_EOF
} Key;

Key term_read_key(void);

#endif

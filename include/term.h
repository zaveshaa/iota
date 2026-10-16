#ifndef TERM_H
#define TERM_H

enum {
    KEY_NONE = 0,
    KEY_ESC = 27,
    KEY_UP = 1000,
    KEY_DOWN = 1001,
    KEY_LEFT = 1002,
    KEY_RIGHT = 1003,
    KEY_MOUSE = 1004
};

typedef struct Term Term;

Term *term_open(void);
void term_close(void);
int term_cols(void);
int term_rows(void);
int term_read_key(void);
int term_keys_released(void);
int term_key_released(void);

void term_mouse_enable(int on);
int term_mouse_x(void);
int term_mouse_y(void);
int term_mouse_button(void);
int term_mouse_wheel(void);
int term_mouse_press(void);
int term_mouse_motion(void);

#endif

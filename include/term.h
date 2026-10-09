#ifndef TERM_H
#define TERM_H

enum {
    KEY_NONE = 0,
    KEY_ESC = 27,
    KEY_UP = 1000,
    KEY_DOWN = 1001,
    KEY_LEFT = 1002,
    KEY_RIGHT = 1003
};

typedef struct Term Term;

Term *term_open(void);
void term_close(void);
int term_cols(void);
int term_rows(void);
int term_read_key(void);
int term_keys_released(void);
int term_key_released(void);

#endif

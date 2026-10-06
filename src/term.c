#define _POSIX_C_SOURCE 200809L

#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include "term.h"

#include <signal.h>
#include <sys/signal.h>
#include <stdio.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved;
static volatile sig_atomic_t dirty = 1;

static void on_winch(int sig) {
    (void)sig;
    dirty = 1;
}

void term_init(void) {
    tcgetattr(STDIN_FILENO, &saved);
    struct termios raw = saved;
    raw.c_lflag &= ~(unsigned)(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    struct sigaction sa = {0};
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);

    fputs("\033[?1049h\033[?25l\033[2J", stdout);
    fflush(stdout);
}

void term_close(void) {
    fputs("\033[?25h\033[?1049l", stdout);
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
}

static void size(int *w, int *h) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0) {
        *w = 80;
        *h = 24;
        return;
    }
    *w = ws.ws_col;
    *h = ws.ws_row;
}

int term_cols(void) {
    int w, h;
    size(&w, &h);
    return w;
}

int term_rows(void) {
    int w, h;
    size(&w, &h);
    return h;
}

int term_dirty(void) { return dirty; }

void term_clean(void) { dirty = 0; }

Key term_read_key(void) {
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    if (poll(&pfd, 1, 16) <= 0) return KEY_NONE;

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_EOF;

    if (c == 27) {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;
        if (seq[0] != '[') return KEY_ESC;
        switch (seq[1]) {
            case 'A': return KEY_TURN_U;
            case 'B': return KEY_TURN_D;
            case 'C': return KEY_TURN_R;
            case 'D': return KEY_TURN_L;
            default:  return KEY_NONE;
        }
    }

    switch (c) {
        case 'w': case 'W': return KEY_W;
        case 'a': case 'A': return KEY_A;
        case 's': case 'S': return KEY_S;
        case 'd': case 'D': return KEY_D;
        case ' ':           return KEY_PLACE;
        case 'x': case 'X': return KEY_DEL;
        case '1':           return KEY_BOX;
        case '2':           return KEY_SPHERE;
        case '3':           return KEY_MESH;
        case ',':           return KEY_PREV;
        case '.':           return KEY_NEXT;
        case 'o': case 'O': return KEY_SAVE;
        case 'l': case 'L': return KEY_LOAD;
        case '\t':          return KEY_TAB;
        case 'r':           return KEY_TURN_OBJ_R;
        case 'R':           return KEY_TURN_OBJ_L;
        case 'y': case 'Y': return KEY_SCALE;
        case '=': case '+': return KEY_ZOOM_IN;
        case '-': case '_': return KEY_ZOOM_OUT;
        default:  return KEY_NONE;
    }
}

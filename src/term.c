#include "term.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct Term {
    struct termios saved;
    int open;
};

static struct Term g_term;

static void term_write(const char *s, size_t n)
{
    size_t off = 0;

    while (off < n) {
        ssize_t wr = write(STDOUT_FILENO, s + off, n - off);

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

static void term_puts(const char *s)
{
    term_write(s, strlen(s));
}

void term_close(void)
{
    if (!g_term.open) {
        return;
    }
    g_term.open = 0;
    term_puts("\x1b[?25h\x1b[?1049l");
    if (tcsetattr(STDIN_FILENO, TCSANOW, &g_term.saved) != 0) {
        /* nothing left to do about it */
    }
}

static void term_on_signal(int sig)
{
    int saved_errno = errno;

    term_close();
    if (signal(sig, SIG_DFL) == SIG_ERR) {
        _exit(128 + sig);
    }
    if (raise(sig) != 0) {
        _exit(128 + sig);
    }
    errno = saved_errno;
}

Term *term_open(void)
{
    static const int fatal[] = {SIGTERM, SIGHUP, SIGINT, SIGSEGV,
                                SIGBUS,  SIGILL, SIGABRT};
    struct termios raw;
    size_t i;

    if (g_term.open) {
        return &g_term;
    }
    if (tcgetattr(STDIN_FILENO, &g_term.saved) != 0) {
        return NULL;
    }
    raw = g_term.saved;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t) ~(OPOST);
    raw.c_cflag |= (tcflag_t)(CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return NULL;
    }
    g_term.open = 1;
    if (atexit(term_close) != 0) {
        term_close();
        return NULL;
    }
    for (i = 0; i < sizeof fatal / sizeof fatal[0]; i++) {
        if (signal(fatal[i], term_on_signal) == SIG_ERR) {
            /* a missing handler only costs us a restored terminal */
        }
    }
    term_puts("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H");
    return &g_term;
}

int term_cols(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    return 80;
}

int term_rows(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return (int)ws.ws_row;
    }
    return 24;
}

int term_read_key(void)
{
    unsigned char buf[8];
    struct pollfd pfd;
    ssize_t n;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) {
        return KEY_NONE;
    }
    n = read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) {
        return KEY_NONE;
    }
    if (buf[0] == 0x1b) {
        if (n == 1) {
            return KEY_ESC;
        }
        if (n >= 3 && buf[1] == '[') {
            switch (buf[2]) {
            case 'A':
                return KEY_UP;
            case 'B':
                return KEY_DOWN;
            case 'C':
                return KEY_RIGHT;
            case 'D':
                return KEY_LEFT;
            default:
                return KEY_NONE;
            }
        }
        return KEY_NONE;
    }
    return (int)buf[0];
}

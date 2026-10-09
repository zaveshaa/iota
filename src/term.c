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

// A terminal has never said a key came back up: it says a key went down and then,
// while it is still down, it says it again, and the silence in between is all a
// program has to go on. That silence is about half a second before the first
// repeat -- long enough to walk across a room before the repeat says the key was
// held and not tapped -- so a keyboard protocol that reports a press, a repeat
// and a release as three events is asked for, and only the terminals that keep
// the flags are believed. The request is sent rather than assumed, because a
// terminal without the protocol prints it on the screen instead of ignoring it.
#define KITTY_SET "\033[=11;1u"
#define KITTY_CLEAR "\033[=0;1u"
#define KITTY_WAIT_MS 120
#define SEQ_WAIT_MS 4

static int g_kitty;
static int g_released;

static void term_write(const char *s, size_t n);

int term_keys_released(void)
{
    return g_kitty;
}

int term_key_released(void)
{
    return g_released;
}

static int poll_byte(char *c, int ms)
{
    struct pollfd pfd;

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, ms) <= 0) {
        return 0;
    }
    return read(STDIN_FILENO, c, 1) == 1;
}

// CSI ? u asks which flags the keyboard is on and the answer is CSI ? flags u.
// Only the shape of the answer is believed, not what it says: the flags in it are
// the ones in use right now, and a terminal nobody has asked anything of is using
// none of them, so a terminal that can do this answers with a zero.
static int ask_kitty(void)
{
    int waited;

    term_write("\033[?u", 4);
    for (waited = 0; waited < KITTY_WAIT_MS;) {
        char c;

        if (!poll_byte(&c, 10)) {
            waited += 10;
            continue;
        }
        if (c != 27) {
            continue;
        }
        if (!poll_byte(&c, SEQ_WAIT_MS) || c != '[') {
            return 0;
        }
        if (!poll_byte(&c, SEQ_WAIT_MS) || c != '?') {
            return 0;
        }
        {
            char last = 0;
            int flags = 0;
            int digits = 0;

            while (poll_byte(&c, SEQ_WAIT_MS)) {
                if (c < '0' || c > '9') {
                    last = c;
                    break;
                }
                if (++digits > 6) {
                    return 0;
                }
                flags = flags * 10 + (c - '0');
            }
            return last == 'u' ? flags : -1;
        }
    }
    return -1;
}

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
    if (g_kitty) {
        term_puts(KITTY_CLEAR);
        g_kitty = 0;
    }
    term_puts("\x1b[?25h\x1b[?1049l");
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_term.saved);
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
        (void)signal(fatal[i], term_on_signal);
    }
    // The screen is entered first, because a terminal keeps a separate stack of
    // the keyboard flags for each screen: flags asked for before the alternate
    // screen is entered are flags asked for on the shell's screen and not on the
    // one that will be playing.
    term_puts("\x1b[?1049h");
    if (ask_kitty() >= 0) {
        int kept;

        term_puts(KITTY_SET);
        kept = ask_kitty();
        if (kept >= 0 && (kept & 2) != 0) {
            g_kitty = 1;
        }
    }
    term_puts("\x1b[?25l\x1b[2J\x1b[H");
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

// A key event in the protocol's own spelling, CSI key ; modifiers : type u for a
// letter and the same with a final letter for an arrow. Type one is a press, two
// is the repeat a terminal has always sent, and three is the release that did not
// exist before, which is the whole reason the protocol was asked for.
static int kitty_key(char first)
{
    int code = -1;
    int mods = 1;
    int type = 1;
    int field = 0;
    int sub = 0;
    int num = 0;
    int digits = 0;
    int have = 0;
    char final = 0;
    char c = first;

    for (;;) {
        if (c >= '0' && c <= '9') {
            if (++digits > 8) {
                return KEY_NONE;
            }
            num = num * 10 + (c - '0');
            have = 1;
        } else if (c == ';' || c == ':') {
            if (field == 0 && sub == 0) {
                code = have ? num : code;
            } else if (field == 1 && sub == 0) {
                mods = num;
            } else if (field == 1 && sub == 1) {
                type = num;
            }
            if (c == ';') {
                field++;
                sub = 0;
            } else {
                sub++;
            }
            num = 0;
            digits = 0;
            have = 0;
        } else if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7e) {
            final = c;
            break;
        } else {
            return KEY_NONE;
        }
        if (!poll_byte(&c, SEQ_WAIT_MS)) {
            return KEY_NONE;
        }
    }
    if (have) {
        if (field == 0 && sub == 0) {
            code = num;
        } else if (field == 1 && sub == 0) {
            mods = num;
        } else if (field == 1 && sub == 1) {
            type = num;
        }
    }
    // shift and the two locks are the same key as their unshifted selves; anything
    // else is somebody else's shortcut and is read and thrown away, because a
    // terminal that reports ctrl+c as a key is not a reason to quit twice
    if (mods < 1 || ((mods - 1) & ~(1 | 64 | 128)) != 0) {
        return KEY_NONE;
    }
    g_released = type == 3;
    switch (final) {
    case 'A':
        return KEY_UP;
    case 'B':
        return KEY_DOWN;
    case 'C':
        return KEY_RIGHT;
    case 'D':
        return KEY_LEFT;
    case 'u':
        return code >= 0 && code <= 255 ? code : KEY_NONE;
    default:
        return KEY_NONE;
    }
}

int term_read_key(void)
{
    char c;

    if (!poll_byte(&c, 0)) {
        return KEY_NONE;
    }
    g_released = 0;
    if (c != 27) {
        return (int)(unsigned char)c;
    }
    {
        char b1;
        char b2;

        if (!poll_byte(&b1, SEQ_WAIT_MS) || !poll_byte(&b2, SEQ_WAIT_MS)) {
            return KEY_ESC;
        }
        if (b1 != '[') {
            return KEY_ESC;
        }
        if (b2 == '?') {
            char d;
            int n = 0;

            // a flags reply, arriving as input because it was asked as input
            while (n < 31 && poll_byte(&d, SEQ_WAIT_MS) && d != 'u') {
                n++;
            }
            return KEY_NONE;
        }
        if (b2 >= '0' && b2 <= '9') {
            return kitty_key(b2);
        }
        switch (b2) {
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
}

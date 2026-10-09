#ifndef INPUT_H
#define INPUT_H

#include "term.h"

#include <string.h>

// A terminal reports a key once and then, while it is held, repeats it -- and
// the gap before the first repeat is half a second on most terminals, which is
// long enough that a lease sized for the repeats runs out inside it and a walk
// stops dead in the middle of a hold. A terminal that reports keys coming up has
// nothing to time: the key is down from its press until its release. Everywhere
// else the lease is the terminal's own repeat delay, learned from the gaps
// between arrivals and never shorter than the gap that just arrived.
#define INPUT_MAX_KEYS      16
#define INPUT_HOLD_TERMINAL 0.25
#define INPUT_HOLD_SLOWEST  0.40

typedef struct {
    int code;
    double hold;
    double last;
    int down;
} InputKey;

typedef struct {
    InputKey keys[INPUT_MAX_KEYS];
    int count;
    double window;
} Input;

static inline void input_clear(Input *in)
{
    memset(in, 0, sizeof *in);
    in->window = INPUT_HOLD_TERMINAL;
}

static inline InputKey *input_key(Input *in, int code)
{
    int i;

    for (i = 0; i < in->count; i++) {
        if (in->keys[i].code == code) {
            return &in->keys[i];
        }
    }
    if (in->count >= INPUT_MAX_KEYS) {
        return NULL;
    }
    in->keys[in->count].code = code;
    return &in->keys[in->count++];
}

// One arrival, be it a press, a repeat or a release. A repeat pushes the lease
// out; a release takes the key down; and the gap between two arrivals widens the
// window so a slow terminal is believed as far as it has shown it needs.
static inline void input_arrive(Input *in, int code, double now)
{
    InputKey *k = input_key(in, code);
    int released = term_key_released();

    if (k == NULL) {
        return;
    }
    if (!released) {
        double gap = now - k->last;

        if (gap > in->window && gap < INPUT_HOLD_SLOWEST) {
            in->window = gap;
        }
    }
    k->last = now;
    k->down = !released;
    k->hold = now + in->window;
}

static inline int input_held(const Input *in, int code, double now)
{
    int i;

    for (i = 0; i < in->count; i++) {
        if (in->keys[i].code != code) {
            continue;
        }
        if (term_keys_released()) {
            return in->keys[i].down;
        }
        return in->keys[i].hold > now;
    }
    return 0;
}

#endif

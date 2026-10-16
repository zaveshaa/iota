#ifndef UI_H
#define UI_H

#include "render.h"

// A tiny immediate-mode widget set, in the spirit of Dear ImGui but sized for a
// character grid: the editor calls the widgets every frame and they draw
// themselves and report what the pointer did to them. A widget that the pointer
// sits on claims the click, so a build in the world never fires through a panel.
// Zero the struct once before its first frame; its fields carry across frames,
// which is how an open value edit survives between polls.
typedef struct {
    Render *r;
    int mx;
    int my;
    int click;
    int down;
    int wheel;
    int hot;
    int active;
    int consumed;
    int editing;
    int edit_len;
    int edit_fresh;
    int edit_commit;
    char edit_buf[24];
} Ui;

void ui_begin(Ui *ui, Render *r, int mx, int my, int click, int down, int wheel);
void ui_end(Ui *ui);

int ui_inside(const Ui *ui, int x, int y, int w, int h);
void ui_panel(Ui *ui, int x, int y, int w, int h, const char *title);
void ui_label(Ui *ui, int x, int y, const char *text, unsigned ink);
int ui_button(Ui *ui, int id, int x, int y, const char *label);
int ui_toggle(Ui *ui, int id, int x, int y, const char *label, int *value);
int ui_step(Ui *ui, int id, int x, int y, int w, const char *label,
            float *value, float step, float lo, float hi, int prec);

// A stepper's value can also be typed straight in: a click on the number opens
// an edit that swallows the keyboard until Enter lands it or Escape drops it.
int ui_edit_key(Ui *ui, int key);
void ui_edit_stop(Ui *ui);

#endif

#ifndef UI_H
#define UI_H

#include "render.h"

// A tiny immediate-mode widget set, in the spirit of Dear ImGui but sized for a
// character grid: the editor calls the widgets every frame and they draw
// themselves and report what the pointer did to them. A widget that the pointer
// sits on claims the click, so a build in the world never fires through a panel.
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

#endif

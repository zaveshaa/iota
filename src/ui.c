#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_BG     RGB(12, 14, 22)
#define UI_EDGE   RGB(70, 96, 130)
#define UI_TITLE  RGB(214, 232, 255)
#define UI_TEXT   RGB(150, 180, 210)
#define UI_BTN    RGB(120, 150, 185)
#define UI_HOT    RGB(255, 232, 150)
#define UI_DOWN   RGB(255, 176, 88)

static float ui_clamp(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void ui_fill(Ui *ui, int x, int y, int w, int h, unsigned ink)
{
    int i;
    int j;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            render_put(ui->r, x + i, y + j, (unsigned char)' ', ink);
        }
    }
}

int ui_inside(const Ui *ui, int x, int y, int w, int h)
{
    return ui->mx >= x && ui->my >= y && ui->mx < x + w && ui->my < y + h;
}

void ui_begin(Ui *ui, Render *r, int mx, int my, int click, int down, int wheel)
{
    ui->r = r;
    ui->mx = mx;
    ui->my = my;
    ui->click = click;
    ui->down = down;
    ui->wheel = wheel;
    ui->hot = 0;
    ui->consumed = 0;
}

void ui_end(Ui *ui)
{
    (void)ui;
}

void ui_panel(Ui *ui, int x, int y, int w, int h, const char *title)
{
    int i;

    ui_fill(ui, x, y, w, h, UI_BG);
    for (i = 0; i < w; i++) {
        render_put(ui->r, x + i, y, (unsigned char)'-', UI_EDGE);
        render_put(ui->r, x + i, y + h - 1, (unsigned char)'-', UI_EDGE);
    }
    for (i = 0; i < h; i++) {
        render_put(ui->r, x, y + i, (unsigned char)'|', UI_EDGE);
        render_put(ui->r, x + w - 1, y + i, (unsigned char)'|', UI_EDGE);
    }
    render_put(ui->r, x, y, (unsigned char)'+', UI_EDGE);
    render_put(ui->r, x + w - 1, y, (unsigned char)'+', UI_EDGE);
    render_put(ui->r, x, y + h - 1, (unsigned char)'+', UI_EDGE);
    render_put(ui->r, x + w - 1, y + h - 1, (unsigned char)'+', UI_EDGE);
    if (title != NULL) {
        render_text(ui->r, x + 2, y, title, UI_TITLE);
    }
    if (ui_inside(ui, x, y, w, h)) {
        ui->consumed = 1;
    }
}

void ui_label(Ui *ui, int x, int y, const char *text, unsigned ink)
{
    render_text(ui->r, x, y, text, ink);
}

int ui_button(Ui *ui, int id, int x, int y, const char *label)
{
    int len = (int)strlen(label);
    int over = ui_inside(ui, x, y, len + 2, 1);
    unsigned ink = over ? (ui->click ? UI_DOWN : UI_HOT) : UI_BTN;

    if (over) {
        ui->hot = id;
        ui->consumed = 1;
    }
    render_put(ui->r, x, y, (unsigned char)'[', ink);
    render_text(ui->r, x + 1, y, label, ink);
    render_put(ui->r, x + len + 1, y, (unsigned char)']', ink);
    return over && ui->click;
}

int ui_toggle(Ui *ui, int id, int x, int y, const char *label, int *value)
{
    int len = (int)strlen(label);
    int over = ui_inside(ui, x, y, len + 4, 1);
    unsigned ink = over ? UI_HOT : UI_BTN;
    int changed = 0;

    if (over) {
        ui->hot = id;
        ui->consumed = 1;
    }
    render_put(ui->r, x, y, (unsigned char)'[', ink);
    render_put(ui->r, x + 1, y, *value ? (unsigned char)'x'
                                       : (unsigned char)' ', ink);
    render_put(ui->r, x + 2, y, (unsigned char)']', ink);
    render_text(ui->r, x + 4, y, label, ink);
    if (over && ui->click) {
        *value = !*value;
        changed = 1;
    }
    return changed;
}

int ui_edit_key(Ui *ui, int key)
{
    if (ui->editing == 0) {
        return 0;
    }
    if (key == 13 || key == 10) {
        ui->edit_commit = 1;
        return 1;
    }
    if (key == 27) {
        ui->editing = 0;
        ui->edit_len = 0;
        ui->edit_fresh = 0;
        ui->edit_buf[0] = '\0';
        return 1;
    }
    if (ui->edit_fresh != 0) {
        ui->edit_len = 0;
        ui->edit_buf[0] = '\0';
        ui->edit_fresh = 0;
    }
    if (key == 127 || key == 8) {
        if (ui->edit_len > 0) {
            ui->edit_len--;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return 1;
    }
    if (key >= 32 && key < 127) {
        if (ui->edit_len < (int)sizeof(ui->edit_buf) - 1) {
            ui->edit_buf[ui->edit_len++] = (char)key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
    }
    return 1;
}

void ui_edit_stop(Ui *ui)
{
    ui->editing = 0;
    ui->edit_len = 0;
    ui->edit_fresh = 0;
    ui->edit_commit = 0;
    ui->edit_buf[0] = '\0';
}

int ui_step(Ui *ui, int id, int x, int y, int w, const char *label,
            float *value, float step, float lo, float hi, int prec)
{
    int lw = (int)strlen(label);
    int mxs = x + lw + 1;
    int pxs = x + w - 1;
    int vxs = mxs + 2;
    int overm;
    int overp;
    unsigned im;
    unsigned ip;
    char buf[28];
    int changed = 0;

    if (ui_inside(ui, x, y, w, 1)) {
        ui->consumed = 1;
    }
    ui_label(ui, x, y, label, UI_TEXT);

    if (ui->editing == id) {
        render_put(ui->r, mxs, y, (unsigned char)'-', UI_BTN);
        render_put(ui->r, pxs, y, (unsigned char)'+', UI_BTN);
        (void)snprintf(buf, sizeof buf, "%s_", ui->edit_buf);
        render_text(ui->r, vxs, y, buf, UI_HOT);
        if (ui->edit_commit != 0) {
            float nv = ui_clamp((float)strtod(ui->edit_buf, NULL), lo, hi);

            if (nv != *value) {
                *value = nv;
                changed = 1;
            }
            ui->editing = 0;
            ui->edit_len = 0;
            ui->edit_fresh = 0;
            ui->edit_commit = 0;
            ui->edit_buf[0] = '\0';
        }
        return changed;
    }

    overm = ui_inside(ui, mxs, y, 1, 1);
    overp = ui_inside(ui, pxs, y, 1, 1);
    im = overm ? UI_HOT : UI_BTN;
    ip = overp ? UI_HOT : UI_BTN;
    if (overm || overp) {
        ui->hot = id;
    }
    render_put(ui->r, mxs, y, (unsigned char)'-', im);
    render_put(ui->r, pxs, y, (unsigned char)'+', ip);
    (void)snprintf(buf, sizeof buf, "%.*f", prec, (double)*value);
    render_text(ui->r, vxs, y, buf, overm || overp ? UI_HOT : UI_TEXT);

    if (overm && ui->click) {
        float nv = ui_clamp(*value - step, lo, hi);

        if (nv != *value) {
            *value = nv;
            changed = 1;
        }
    } else if (overp && ui->click) {
        float nv = ui_clamp(*value + step, lo, hi);

        if (nv != *value) {
            *value = nv;
            changed = 1;
        }
    } else if (ui->click && ui_inside(ui, vxs, y, pxs - vxs, 1)) {
        ui->editing = id;
        (void)snprintf(ui->edit_buf, sizeof ui->edit_buf, "%.*f", prec,
                       (double)*value);
        ui->edit_len = (int)strlen(ui->edit_buf);
        ui->edit_fresh = 1;
        ui->edit_commit = 0;
    }
    return changed;
}

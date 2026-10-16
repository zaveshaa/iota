#include "ui.h"

#include <stdio.h>

static int g_checks;
static int g_failures;

static void check(int ok, const char *name)
{
    g_checks++;
    if (ok) {
        (void)printf("ok   %s\n", name);
        return;
    }
    g_failures++;
    (void)printf("FAIL %s\n", name);
}

static void button_under_pointer(void)
{
    Render *r = render_open(40, 10);
    Ui ui;
    int hit;

    ui_begin(&ui, r, 2, 3, 1, 1, 0);
    hit = ui_button(&ui, 7, 1, 3, "box");
    ui_end(&ui);
    check(hit == 1, "a button the pointer is on reports the click");
    check(ui.consumed == 1, "a button the pointer is on claims the click");
    check(ui.hot == 7, "the button remembers it was the hot one");
    render_close(r);
}

static void button_away_from_pointer(void)
{
    Render *r = render_open(40, 10);
    Ui ui;
    int hit;

    ui_begin(&ui, r, 2, 3, 1, 1, 0);
    hit = ui_button(&ui, 7, 1, 8, "box");
    ui_end(&ui);
    check(hit == 0, "a button elsewhere ignores the click");
    check(ui.consumed == 0, "an untouched button claims nothing");
    render_close(r);
}

static void panel_claims_its_ground(void)
{
    Render *r = render_open(40, 10);
    Ui ui;

    ui_begin(&ui, r, 5, 5, 0, 0, 0);
    ui_panel(&ui, 0, 0, 20, 8, "panel");
    ui_end(&ui);
    check(ui.consumed == 1, "the pointer inside a panel is claimed");

    ui_begin(&ui, r, 30, 9, 0, 0, 0);
    ui_panel(&ui, 0, 0, 20, 8, "panel");
    ui_end(&ui);
    check(ui.consumed == 0, "the pointer outside a panel is free");
    render_close(r);
}

static void stepper_steps_and_clamps(void)
{
    Render *r = render_open(40, 10);
    Ui ui;
    float v = 1.0f;
    int changed;

    ui_begin(&ui, r, 30, 2, 1, 1, 0);
    changed = ui_step(&ui, 9, 1, 2, 30, "size", &v, 0.5f, 0.0f, 2.0f, 1);
    ui_end(&ui);
    check(changed == 1 && v == 1.5f, "a plus press raises the value");
    check(ui.consumed == 1, "a step claims a press inside its span");

    v = 0.0f;
    ui_begin(&ui, r, 6, 2, 1, 1, 0);
    changed = ui_step(&ui, 9, 1, 2, 30, "size", &v, 0.5f, 0.0f, 2.0f, 1);
    ui_end(&ui);
    check(changed == 0 && v == 0.0f, "a minus press holds at the floor");
    render_close(r);
}

static void toggle_flips(void)
{
    Render *r = render_open(40, 10);
    Ui ui;
    int on = 0;
    int changed;

    ui_begin(&ui, r, 2, 4, 1, 1, 0);
    changed = ui_toggle(&ui, 3, 1, 4, "grid", &on);
    ui_end(&ui);
    check(changed == 1 && on == 1, "a toggle flips and says so");
    render_close(r);
}

int main(void)
{
    button_under_pointer();
    button_away_from_pointer();
    panel_claims_its_ground();
    stepper_steps_and_clamps();
    toggle_flips();
    (void)printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}

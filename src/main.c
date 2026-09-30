#include "camera.h"
#include "render.h"
#include "term.h"
#include "vec3.h"

#define FOV    1.1f
#define STEP   0.12f
#define TURN   0.03f
#define PITCH_MAX 1.5f

static void draw_floor(Camera cam, int cols, int rows) {
    Vec3 fwd, right, up;
    cam_basis(cam, &fwd, &right, &up);

    float aspect = (float)cols / (float)rows * 0.5f;

    for (int row = 0; row < rows; row++) {
        float sy = 1.0f - 2.0f * (float)row / (float)(rows - 1);
        for (int col = 0; col < cols; col++) {
            float sx = (2.0f * (float)col / (float)(cols - 1) - 1.0f) * aspect;
            Vec3 dir = v3_norm(v3_add(fwd, v3_add(v3_scale(right, sx * FOV), v3_scale(up, sy * FOV))));

            float t = dir.y < -0.0001f ? -cam.pos.y / dir.y : -1.0f;
            if (t <= 0.0f || t > 120.0f) {
                render_put(col, row, RENDER_SKY, 0.0f);
                continue;
            }

            Vec3 hit = v3_add(cam.pos, v3_scale(dir, t));
            float gx = __builtin_fabsf(hit.x - __builtin_roundf(hit.x));
            float gz = __builtin_fabsf(hit.z - __builtin_roundf(hit.z));
            float g = gx < gz ? gx : gz;

            float fog = 1.0f - t / 32.0f;
            char ch = (g > 0.015f && g < 0.035f) ? '|' : RENDER_RAMP[render_shade(fog * 0.7f)];
            render_put(col, row, ch, t);
        }
    }
}

static int handle(Camera *cam, Key k) {
    Vec3 fwd, right, up;
    cam_basis(*cam, &fwd, &right, &up);

    switch (k) {
        case KEY_W:      cam->pos = v3_add(cam->pos, v3_scale(fwd, STEP)); break;
        case KEY_S:      cam->pos = v3_sub(cam->pos, v3_scale(fwd, STEP)); break;
        case KEY_A:      cam->pos = v3_sub(cam->pos, v3_scale(right, STEP)); break;
        case KEY_D:      cam->pos = v3_add(cam->pos, v3_scale(right, STEP)); break;
        case KEY_TURN_L: cam->yaw -= TURN; break;
        case KEY_TURN_R: cam->yaw += TURN; break;
        case KEY_TURN_U: cam->pitch += TURN; break;
        case KEY_TURN_D: cam->pitch -= TURN; break;
        case KEY_ESC:    return 0;
        default: break;
    }

    if (cam->pitch > PITCH_MAX)  cam->pitch = PITCH_MAX;
    if (cam->pitch < -PITCH_MAX) cam->pitch = -PITCH_MAX;
    return 1;
}

int main(void) {
    term_init();

    Camera cam = {v3(0.0f, 1.7f, 0.0f), 0.0f, 0.0f};
    int cols = term_cols(), rows = term_rows();
    render_init(cols, rows);

    for (;;) {
        if (term_dirty()) {
            int c = term_cols(), r = term_rows();
            if (c != cols || r != rows) {
                render_resize(c, r);
                cols = c;
                rows = r;
            }
            term_clean();
        }

        if (!handle(&cam, term_read_key())) break;

        draw_floor(cam, cols, rows);
        render_present();
    }

    render_resize(0, 0);
    term_close();
    return 0;
}

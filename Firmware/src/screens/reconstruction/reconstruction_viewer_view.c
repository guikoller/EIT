#include "reconstruction_viewer_view.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define DISPLAY_SIZE 288

static void return_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_return) {
        view->bindings.on_return(view->bindings.ctx);
    }
}

static void save_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_save) {
        view->bindings.on_save(view->bindings.ctx);
    }
}

static void play_pause_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_play_pause) {
        view->bindings.on_play_pause(view->bindings.ctx);
    }
}

static void create_canvas(reconstruction_viewer_view_t *view)
{
    view->canvas = lv_canvas_create(view->cont);

    static lv_color_t canvas_buf[DISPLAY_SIZE * DISPLAY_SIZE];
    lv_canvas_set_buffer(view->canvas, canvas_buf, DISPLAY_SIZE, DISPLAY_SIZE, LV_COLOR_FORMAT_RGB565);

    lv_obj_align(view->canvas, LV_ALIGN_CENTER, 0, 20);
    lv_canvas_fill_bg(view->canvas, lv_color_hex(0x000000), LV_OPA_COVER);
}

static void add_electrode_markers(reconstruction_viewer_view_t *view)
{
    const int center_x = DISPLAY_SIZE / 2;
    const int center_y = DISPLAY_SIZE / 2;
    const int radius = DISPLAY_SIZE / 2 + 20;

    for (int i = 0; i < 16; i++) {
        const float angle = (i * 2.0f * 3.14159f) / 16.0f;
        const int x = center_x + (int)(radius * cosf(angle));
        const int y = center_y - (int)(radius * sinf(angle));

        lv_obj_t *label = lv_label_create(view->cont);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_label_set_text(label, num);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

        lv_obj_align_to(label, view->canvas, LV_ALIGN_TOP_LEFT, x - 8, y - 8);
    }
}

void reconstruction_viewer_view_create(reconstruction_viewer_view_t *view, lv_obj_t *parent, const reconstruction_view_bindings_t *bindings)
{
    if (!view || !parent) return;
    memset(view, 0, sizeof(*view));

    if (bindings) {
        view->bindings = *bindings;
    } else {
        memset(&view->bindings, 0, sizeof(view->bindings));
    }

    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(view->cont, LV_ALIGN_TOP_LEFT, 0, 0);

    view->label_title = lv_label_create(view->cont);
    lv_label_set_text(view->label_title, "");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 10);

    view->label_status = lv_label_create(view->cont);
    lv_label_set_text(view->label_status, "");
    lv_obj_set_style_text_color(view->label_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_status, LV_ALIGN_TOP_MID, 0, 40);

    view->label_fps = lv_label_create(view->cont);
    lv_label_set_text(view->label_fps, "-- FPS");
    lv_obj_set_style_text_color(view->label_fps, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_text_font(view->label_fps, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_fps, LV_ALIGN_TOP_RIGHT, -15, 15);

    create_canvas(view);
    add_electrode_markers(view);

    lv_obj_t *btn_return = lv_button_create(view->cont);
    lv_obj_set_size(btn_return, 180, 50);
    lv_obj_align(btn_return, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_return, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(btn_return, 5, 0);
    lv_obj_add_event_cb(btn_return, return_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_return = lv_label_create(btn_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " RETURN");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);

    view->btn_play_pause = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_play_pause, 140, 50);
    lv_obj_align(view->btn_play_pause, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0x2a9d2a), 0);
    lv_obj_set_style_radius(view->btn_play_pause, 5, 0);
    lv_obj_add_event_cb(view->btn_play_pause, play_pause_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_play_pause_text = lv_label_create(view->btn_play_pause);
    lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY " PLAY");
    lv_obj_set_style_text_color(view->label_play_pause_text, lv_color_white(), 0);
    lv_obj_center(view->label_play_pause_text);

    view->btn_save = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_save, 180, 50);
    lv_obj_align(view->btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(view->btn_save, lv_color_hex(0x2a7da8), 0);
    lv_obj_set_style_radius(view->btn_save, 5, 0);
    lv_obj_add_event_cb(view->btn_save, save_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_save = lv_label_create(view->btn_save);
    lv_label_set_text(label_save, "SAVE IMAGE");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_center(label_save);
}

void reconstruction_viewer_view_set_title(reconstruction_viewer_view_t *view, const char *filename)
{
    if (!view || !view->label_title) return;

    char title_text[256];
    snprintf(title_text, sizeof(title_text), "EIT Reconstruction - %s", filename ? filename : "");
    lv_label_set_text(view->label_title, title_text);
}

void reconstruction_viewer_view_set_status(reconstruction_viewer_view_t *view, const char *text)
{
    if (!view || !view->label_status) return;
    lv_label_set_text(view->label_status, text ? text : "");
}

void reconstruction_viewer_view_set_save_enabled(reconstruction_viewer_view_t *view, int enabled)
{
    if (!view || !view->btn_save) return;

    if (enabled) {
        lv_obj_clear_state(view->btn_save, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(view->btn_save, LV_STATE_DISABLED);
    }
}

void reconstruction_viewer_view_render_rgb565(reconstruction_viewer_view_t *view, const uint16_t *rgb565, uint32_t width, uint32_t height)
{
    if (!view || !view->canvas || !rgb565) return;
    if (width != DISPLAY_SIZE || height != DISPLAY_SIZE) return;

    lv_color_t *canvas_buf = (lv_color_t *)lv_canvas_get_buf(view->canvas);
    if (!canvas_buf) return;

    memcpy(canvas_buf, rgb565, DISPLAY_SIZE * DISPLAY_SIZE * sizeof(uint16_t));

    uint16_t *buf16 = (uint16_t *)canvas_buf;
    const uint16_t white_color = 0xFFFF;
    const int center = DISPLAY_SIZE / 2;
    const int radius = DISPLAY_SIZE / 2;

    for (int angle_deg = 0; angle_deg < 360; angle_deg++) {
        const float angle = angle_deg * 3.14159f / 180.0f;
        for (int r = radius - 2; r <= radius - 1; r++) {
            const int x = center + (int)(r * cosf(angle));
            const int y = center - (int)(r * sinf(angle));
            if (x >= 0 && x < DISPLAY_SIZE && y >= 0 && y < DISPLAY_SIZE) {
                buf16[y * DISPLAY_SIZE + x] = white_color;
            }
        }
    }

    const int electrode_radius = DISPLAY_SIZE / 2 - 1;
    for (int i = 0; i < 16; i++) {
        const float angle = (i * 2.0f * 3.14159f) / 16.0f;
        const int ex = center + (int)(electrode_radius * cosf(angle));
        const int ey = center - (int)(electrode_radius * sinf(angle));

        for (int dy = -5; dy <= 5; dy++) {
            for (int dx = -5; dx <= 5; dx++) {
                if (dx * dx + dy * dy <= 25) {
                    const int px = ex + dx;
                    const int py = ey + dy;
                    if (px >= 0 && px < DISPLAY_SIZE && py >= 0 && py < DISPLAY_SIZE) {
                        buf16[py * DISPLAY_SIZE + px] = white_color;
                    }
                }
            }
        }
    }

    lv_obj_invalidate(view->canvas);
}

void reconstruction_viewer_view_set_fps(reconstruction_viewer_view_t *view, uint32_t fps_x10)
{
    if (!view || !view->label_fps) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu.%lu FPS",
             (unsigned long)(fps_x10 / 10u),
             (unsigned long)(fps_x10 % 10u));
    lv_label_set_text(view->label_fps, buf);
}

void reconstruction_viewer_view_set_play_state(reconstruction_viewer_view_t *view, int playing)
{
    if (!view || !view->btn_play_pause || !view->label_play_pause_text) return;

    if (playing) {
        lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PAUSE " PAUSE");
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0xd44a00), 0);
    } else {
        lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY " PLAY");
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0x2a9d2a), 0);
    }
}

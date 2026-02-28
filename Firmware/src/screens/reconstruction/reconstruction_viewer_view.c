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

static void noise_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_noise_toggle) {
        view->bindings.on_noise_toggle(view->bindings.ctx);
    }
}

static void noise_slider_changed(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    int32_t val = lv_slider_get_value(view->slider_noise);
    if (view->bindings.on_noise_level) {
        view->bindings.on_noise_level(view->bindings.ctx, val);
    }
}

static void create_canvas(reconstruction_viewer_view_t *view)
{
    view->canvas = lv_canvas_create(view->cont);

    static lv_color_t canvas_buf[DISPLAY_SIZE * DISPLAY_SIZE];
    lv_canvas_set_buffer(view->canvas, canvas_buf, DISPLAY_SIZE, DISPLAY_SIZE, LV_COLOR_FORMAT_RGB565);

    lv_obj_align(view->canvas, LV_ALIGN_CENTER, -30, 20);
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

    /* ---- Left-side controls (play/pause, noise toggle) ---- */
    view->btn_play_pause = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_play_pause, 60, 50);
    lv_obj_align(view->btn_play_pause, LV_ALIGN_LEFT_MID, 10, -20);
    lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0x2a9d2a), 0);
    lv_obj_set_style_radius(view->btn_play_pause, 5, 0);
    lv_obj_add_event_cb(view->btn_play_pause, play_pause_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_play_pause_text = lv_label_create(view->btn_play_pause);
    lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(view->label_play_pause_text, lv_color_white(), 0);
    lv_obj_center(view->label_play_pause_text);

    view->btn_noise = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_noise, 60, 40);
    lv_obj_align(view->btn_noise, LV_ALIGN_LEFT_MID, 10, 40);
    lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(view->btn_noise, 5, 0);
    lv_obj_add_event_cb(view->btn_noise, noise_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_noise_text = lv_label_create(view->btn_noise);
    lv_label_set_text(view->label_noise_text, "N:OFF");
    lv_obj_set_style_text_color(view->label_noise_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label_noise_text, &lv_font_montserrat_14, 0);
    lv_obj_center(view->label_noise_text);

    /* ---- Right-side vertical noise slider ---- */
    view->slider_noise = lv_slider_create(view->cont);
    lv_obj_set_size(view->slider_noise, 30, 200);
    lv_obj_align(view->slider_noise, LV_ALIGN_RIGHT_MID, -30, 10);
    lv_slider_set_range(view->slider_noise, 0, 100);
    lv_slider_set_value(view->slider_noise, 10, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_hex(0xd4aa00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_white(), LV_PART_KNOB);
    lv_obj_add_event_cb(view->slider_noise, noise_slider_changed, LV_EVENT_VALUE_CHANGED, view);

    /* Noise percentage label (above slider) */
    view->label_noise_pct = lv_label_create(view->cont);
    lv_label_set_text(view->label_noise_pct, "10%");
    lv_obj_set_style_text_color(view->label_noise_pct, lv_color_hex(0xd4aa00), 0);
    lv_obj_set_style_text_font(view->label_noise_pct, &lv_font_montserrat_14, 0);
    lv_obj_align_to(view->label_noise_pct, view->slider_noise, LV_ALIGN_OUT_TOP_MID, 0, -8);

    /* Slider label at bottom */
    lv_obj_t *label_slider_title = lv_label_create(view->cont);
    lv_label_set_text(label_slider_title, "NOISE");
    lv_obj_set_style_text_color(label_slider_title, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(label_slider_title, &lv_font_montserrat_14, 0);
    lv_obj_align_to(label_slider_title, view->slider_noise, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    /* ---- Bottom bar: RETURN (left) and SAVE (right) ---- */
    lv_obj_t *btn_return = lv_button_create(view->cont);
    lv_obj_set_size(btn_return, 160, 45);
    lv_obj_align(btn_return, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_set_style_bg_color(btn_return, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(btn_return, 5, 0);
    lv_obj_add_event_cb(btn_return, return_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_return = lv_label_create(btn_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " RETURN");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);

    view->btn_save = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_save, 160, 45);
    lv_obj_align(view->btn_save, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
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

    /* Draw circle border using finer angle steps to avoid gaps */
    const int num_steps = 1440;  /* 0.25-degree steps for a continuous line */
    for (int s = 0; s < num_steps; s++) {
        const float angle = s * (2.0f * 3.14159f) / num_steps;
        for (int r = radius - 3; r <= radius - 1; r++) {
            const int x = center + (int)(r * cosf(angle));
            const int y = center - (int)(r * sinf(angle));
            if (x >= 0 && x < DISPLAY_SIZE && y >= 0 && y < DISPLAY_SIZE) {
                buf16[y * DISPLAY_SIZE + x] = white_color;
            }
        }
    }

    /* Electrode dots – centered on the circle border line (radius - 2) */
    const int dot_r = 5;
    const int electrode_radius = radius - 2;
    for (int i = 0; i < 16; i++) {
        const float angle = (i * 2.0f * 3.14159f) / 16.0f;
        const int ex = center + (int)(electrode_radius * cosf(angle));
        const int ey = center - (int)(electrode_radius * sinf(angle));

        for (int dy = -dot_r; dy <= dot_r; dy++) {
            for (int dx = -dot_r; dx <= dot_r; dx++) {
                if (dx * dx + dy * dy <= dot_r * dot_r) {
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
        lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PAUSE);
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0xd44a00), 0);
    } else {
        lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(0x2a9d2a), 0);
    }
}

void reconstruction_viewer_view_set_noise_state(reconstruction_viewer_view_t *view, int enabled)
{
    if (!view || !view->btn_noise || !view->label_noise_text) return;

    if (enabled) {
        lv_label_set_text(view->label_noise_text, "N:ON");
        lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(0xd4aa00), 0);
    } else {
        lv_label_set_text(view->label_noise_text, "N:OFF");
        lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(0x555555), 0);
    }
}

void reconstruction_viewer_view_set_noise_level(reconstruction_viewer_view_t *view, int32_t pct)
{
    if (!view || !view->label_noise_pct) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", (long)pct);
    lv_label_set_text(view->label_noise_pct, buf);
}

#include "reconstruction_viewer_view.h"

#include "eit_config.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define DISPLAY_SIZE_MAX  EIT_DISPLAY_SIZE_MAX
#define NUM_ELEC          EIT_NUM_ELECTRODES
#define PI_F              3.14159265f

/* ---- Light theme color palette ---- */
#define COL_BG          0xf5f5f5
#define COL_TEXT        0x333333
#define COL_TEXT_SEC    0x666666
#define COL_ACCENT      0x4a9fd8
#define COL_BTN         0x2a7da8
#define COL_BTN_SEC     0x888888
#define COL_BTN_PLAY    0x2e7d32
#define COL_BTN_PAUSE   0xc62828
#define COL_SLIDER_BG   0xe0e0e0
#define COL_SLIDER_IND  0xd4aa00
#define COL_FPS         0x2e7d32
#define COL_ALGO        0xb8860b

/* ---- Pre-computed overlay mask (recomputed when display_size changes) ---- */
/* Bitmask: 1 bit per pixel, packed into uint32_t words (sized for max) */
#define OVERLAY_WORDS_MAX ((DISPLAY_SIZE_MAX * DISPLAY_SIZE_MAX + 31u) / 32u)
static uint32_t s_overlay_mask[OVERLAY_WORDS_MAX];
static int s_overlay_ready = 0;
static uint32_t s_overlay_size = 0;  /* the display_size the mask was built for */

static void overlay_set_pixel(int x, int y, uint32_t dsz)
{
    if (x < 0 || x >= (int)dsz || y < 0 || y >= (int)dsz) return;
    uint32_t idx = (uint32_t)(y * (int)dsz + x);
    s_overlay_mask[idx >> 5] |= (1u << (idx & 31u));
}

static void precompute_overlay(uint32_t dsz)
{
    if (s_overlay_ready && s_overlay_size == dsz) return;
    memset(s_overlay_mask, 0, sizeof(s_overlay_mask));

    const int center = (int)dsz / 2;
    const int radius = (int)dsz / 2;

    /* Circle border — fine angle steps, 3-pixel wide line */
    const int num_steps = 1440;
    for (int s = 0; s < num_steps; s++) {
        const float angle = s * (2.0f * PI_F) / num_steps;
        const float ca = cosf(angle);
        const float sa = sinf(angle);
        for (int r = radius - 3; r <= radius - 1; r++) {
            overlay_set_pixel(center + (int)(r * ca), center - (int)(r * sa), dsz);
        }
    }

    /* Electrode dots — centered on circle border line */
    const int dot_r = 5;
    const int electrode_radius = radius - 2;
    for (int i = 0; i < (int)NUM_ELEC; i++) {
        const float angle = (i * 2.0f * PI_F) / (float)NUM_ELEC;
        const int ex = center + (int)(electrode_radius * cosf(angle));
        const int ey = center - (int)(electrode_radius * sinf(angle));

        for (int dy = -dot_r; dy <= dot_r; dy++) {
            for (int dx = -dot_r; dx <= dot_r; dx++) {
                if (dx * dx + dy * dy <= dot_r * dot_r) {
                    overlay_set_pixel(ex + dx, ey + dy, dsz);
                }
            }
        }
    }

    s_overlay_size = dsz;
    s_overlay_ready = 1;
}

static void nav_return_clicked(void *ctx)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)ctx;
    if (!view) return;

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

static void record_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_record) {
        view->bindings.on_record(view->bindings.ctx);
    }
}

static void send_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_send) {
        view->bindings.on_send(view->bindings.ctx);
    }
}

static void stream_btn_clicked(lv_event_t *e)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (view->bindings.on_stream) {
        view->bindings.on_stream(view->bindings.ctx);
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

static void nav_home_clicked(void *ctx)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)ctx;
    if (view && view->bindings.on_nav_home) {
        view->bindings.on_nav_home(view->bindings.ctx);
    }
}

static void nav_eit_clicked(void *ctx)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)ctx;
    if (view && view->bindings.on_nav_eit) {
        view->bindings.on_nav_eit(view->bindings.ctx);
    }
}

static void nav_settings_clicked(void *ctx)
{
    reconstruction_viewer_view_t *view = (reconstruction_viewer_view_t *)ctx;
    if (view && view->bindings.on_nav_settings) {
        view->bindings.on_nav_settings(view->bindings.ctx);
    }
}

static void create_canvas(reconstruction_viewer_view_t *view)
{
    uint32_t dsz = view->display_size;
    if (dsz == 0) dsz = EIT_DISPLAY_SIZE;

    view->canvas = lv_canvas_create(view->content);

    /* Canvas pixel buffer lives in SDRAM (sized for the max option) */
    lv_color_t *canvas_buf = (lv_color_t *)EIT_SDRAM_CANVAS_BUF_ADDR;
    lv_canvas_set_buffer(view->canvas, canvas_buf, dsz, dsz, LV_COLOR_FORMAT_RGB565);

    lv_obj_align(view->canvas, LV_ALIGN_CENTER, 0, 12);
    lv_canvas_fill_bg(view->canvas, lv_color_hex(COL_BG), LV_OPA_COVER);
}

static void add_electrode_markers(reconstruction_viewer_view_t *view)
{
    const uint32_t dsz = view->display_size ? view->display_size : EIT_DISPLAY_SIZE;
    const int center_x = (int)dsz / 2;
    const int center_y = (int)dsz / 2;
    const int radius = (int)dsz / 2 + 20;

    for (int i = 0; i < (int)NUM_ELEC; i++) {
        const float angle = (i * 2.0f * PI_F) / (float)NUM_ELEC;
        const int x = center_x + (int)(radius * cosf(angle));
        const int y = center_y - (int)(radius * sinf(angle));

        lv_obj_t *label = lv_label_create(view->content);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_label_set_text(label, num);
        lv_obj_set_style_text_color(label, lv_color_hex(COL_TEXT_SEC), 0);
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

    /* Set default display size (presenter may override via set_display_size) */
    view->display_size = EIT_DISPLAY_SIZE;

    /* Full-screen container - light background */
    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(view->cont, LV_ALIGN_TOP_LEFT, 0, 0);

    left_menu_bindings_t menu_bindings;
    memset(&menu_bindings, 0, sizeof(menu_bindings));
    menu_bindings.ctx = view;
    menu_bindings.on_home = nav_home_clicked;
    menu_bindings.on_eit = nav_eit_clicked;
    menu_bindings.on_settings = nav_settings_clicked;
    menu_bindings.on_return = nav_return_clicked;
    left_menu_create(&view->menu, view->cont, LEFT_MENU_ITEM_EIT, &menu_bindings);

    view->content = lv_obj_create(view->cont);
    lv_obj_set_size(view->content, left_menu_content_width(), LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, left_menu_content_x(), 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label */
    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, "");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 10);

    /* Status label */
    view->label_status = lv_label_create(view->content);
    lv_label_set_text(view->label_status, "");
    lv_obj_set_style_text_color(view->label_status, lv_color_hex(COL_TEXT_SEC), 0);
    lv_obj_set_style_text_font(view->label_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_status, LV_ALIGN_TOP_MID, 0, 40);

    /* FPS label */
    view->label_fps = lv_label_create(view->content);
    lv_label_set_text(view->label_fps, "-- FPS");
    lv_obj_set_style_text_color(view->label_fps, lv_color_hex(COL_FPS), 0);
    lv_obj_set_style_text_font(view->label_fps, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_fps, LV_ALIGN_TOP_RIGHT, -15, 15);

    /* Algorithm label */
    view->label_algo = lv_label_create(view->content);
    lv_label_set_text(view->label_algo, "");
    lv_obj_set_style_text_color(view->label_algo, lv_color_hex(COL_ALGO), 0);
    lv_obj_set_style_text_font(view->label_algo, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_algo, LV_ALIGN_TOP_LEFT, 15, 15);

    create_canvas(view);
    add_electrode_markers(view);

    /* ---- Left-side controls (play/pause, noise toggle) ---- */
    view->btn_play_pause = lv_button_create(view->content);
    lv_obj_set_size(view->btn_play_pause, 80, 60);
    lv_obj_align(view->btn_play_pause, LV_ALIGN_LEFT_MID, 10, -25);
    lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(COL_BTN_PLAY), 0);
    lv_obj_set_style_radius(view->btn_play_pause, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_play_pause, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_play_pause, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_play_pause, 2, 0);
    lv_obj_add_event_cb(view->btn_play_pause, play_pause_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_play_pause_text = lv_label_create(view->btn_play_pause);
    lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(view->label_play_pause_text, lv_color_white(), 0);
    lv_obj_center(view->label_play_pause_text);

    view->btn_noise = lv_button_create(view->content);
    lv_obj_set_size(view->btn_noise, 80, 55);
    lv_obj_align(view->btn_noise, LV_ALIGN_LEFT_MID, 10, 45);
    lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(COL_BTN_SEC), 0);
    lv_obj_set_style_radius(view->btn_noise, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_noise, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_noise, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_noise, 2, 0);
    lv_obj_add_event_cb(view->btn_noise, noise_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_noise_text = lv_label_create(view->btn_noise);
    lv_label_set_text(view->label_noise_text, "N:OFF");
    lv_obj_set_style_text_color(view->label_noise_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label_noise_text, &lv_font_montserrat_14, 0);
    lv_obj_center(view->label_noise_text);

    /* ---- Right-side vertical noise slider ---- */
    view->slider_noise = lv_slider_create(view->content);
    lv_obj_set_size(view->slider_noise, 30, 200);
    lv_obj_align(view->slider_noise, LV_ALIGN_RIGHT_MID, -22, 10);
    lv_slider_set_range(view->slider_noise, 0, 100);
    lv_slider_set_value(view->slider_noise, 10, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_hex(COL_SLIDER_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_hex(COL_SLIDER_IND), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view->slider_noise, lv_color_hex(COL_BTN), LV_PART_KNOB);
    lv_obj_add_event_cb(view->slider_noise, noise_slider_changed, LV_EVENT_VALUE_CHANGED, view);

    /* Noise percentage label (above slider) */
    view->label_noise_pct = lv_label_create(view->content);
    lv_label_set_text(view->label_noise_pct, "10%");
    lv_obj_set_style_text_color(view->label_noise_pct, lv_color_hex(COL_SLIDER_IND), 0);
    lv_obj_set_style_text_font(view->label_noise_pct, &lv_font_montserrat_14, 0);
    lv_obj_align_to(view->label_noise_pct, view->slider_noise, LV_ALIGN_OUT_TOP_MID, 0, -8);

    /* Slider label at bottom */
    lv_obj_t *label_slider_title = lv_label_create(view->content);
    lv_label_set_text(label_slider_title, "NOISE");
    lv_obj_set_style_text_color(label_slider_title, lv_color_hex(COL_TEXT_SEC), 0);
    lv_obj_set_style_text_font(label_slider_title, &lv_font_montserrat_14, 0);
    lv_obj_align_to(label_slider_title, view->slider_noise, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    /* ---- Bottom bar: RECORD, SAVE, SEND ---- */
    view->btn_record = lv_button_create(view->content);
    lv_obj_set_size(view->btn_record, 110, 52);
    lv_obj_align(view->btn_record, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(view->btn_record, lv_color_hex(0xc62828), 0);  /* Red for record */
    lv_obj_set_style_radius(view->btn_record, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_record, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_record, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_record, 2, 0);
    lv_obj_add_event_cb(view->btn_record, record_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_record = lv_label_create(view->btn_record);
    lv_label_set_text(label_record, LV_SYMBOL_DOWNLOAD " REC JSON");
    lv_obj_set_style_text_color(label_record, lv_color_white(), 0);
    lv_obj_center(label_record);

    /* STREAM toggle button — next to RECORD */
    view->btn_stream = lv_button_create(view->content);
    lv_obj_set_size(view->btn_stream, 110, 52);
    lv_obj_align(view->btn_stream, LV_ALIGN_BOTTOM_LEFT, 130, -10);
    lv_obj_set_style_bg_color(view->btn_stream, lv_color_hex(COL_BTN_SEC), 0);
    lv_obj_set_style_radius(view->btn_stream, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_stream, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_stream, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_stream, 2, 0);
    lv_obj_add_event_cb(view->btn_stream, stream_btn_clicked, LV_EVENT_CLICKED, view);

    view->label_stream_text = lv_label_create(view->btn_stream);
    lv_label_set_text(view->label_stream_text, LV_SYMBOL_WIFI " STREAM");
    lv_obj_set_style_text_color(view->label_stream_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label_stream_text, &lv_font_montserrat_14, 0);
    lv_obj_center(view->label_stream_text);

    view->btn_save = lv_button_create(view->content);
    lv_obj_set_size(view->btn_save, 110, 52);
    lv_obj_align(view->btn_save, LV_ALIGN_BOTTOM_RIGHT, -128, -10);
    lv_obj_set_style_bg_color(view->btn_save, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_save, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_save, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_save, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_save, 2, 0);
    lv_obj_add_event_cb(view->btn_save, save_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_save = lv_label_create(view->btn_save);
    lv_label_set_text(label_save, LV_SYMBOL_SAVE " SAVE");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_center(label_save);

    view->btn_send = lv_button_create(view->content);
    lv_obj_set_size(view->btn_send, 110, 52);
    lv_obj_align(view->btn_send, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_set_style_bg_color(view->btn_send, lv_color_hex(COL_BTN_PLAY), 0);
    lv_obj_set_style_radius(view->btn_send, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_send, 4, 0);
    lv_obj_set_style_shadow_color(view->btn_send, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_send, 2, 0);
    lv_obj_add_event_cb(view->btn_send, send_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_send = lv_label_create(view->btn_send);
    lv_label_set_text(label_send, LV_SYMBOL_UPLOAD " SEND");
    lv_obj_set_style_text_color(label_send, lv_color_white(), 0);
    lv_obj_center(label_send);
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

void reconstruction_viewer_view_set_record_enabled(reconstruction_viewer_view_t *view, int enabled)
{
    if (!view || !view->btn_record) return;

    if (enabled) {
        lv_obj_clear_state(view->btn_record, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(view->btn_record, LV_STATE_DISABLED);
    }
}

void reconstruction_viewer_view_set_send_enabled(reconstruction_viewer_view_t *view, int enabled)
{
    if (!view || !view->btn_send) return;

    if (enabled) {
        lv_obj_clear_state(view->btn_send, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(view->btn_send, LV_STATE_DISABLED);
    }
}

void reconstruction_viewer_view_render_rgb565(reconstruction_viewer_view_t *view, const uint16_t *rgb565, uint32_t width, uint32_t height)
{
    if (!view || !view->canvas || !rgb565) return;
    uint32_t dsz = view->display_size ? view->display_size : EIT_DISPLAY_SIZE;
    if (width != dsz || height != dsz) return;

    lv_color_t *canvas_buf = (lv_color_t *)lv_canvas_get_buf(view->canvas);
    if (!canvas_buf) return;

    memcpy(canvas_buf, rgb565, dsz * dsz * sizeof(uint16_t));

    /* Apply pre-computed overlay mask (fast bit-test loop, no trig) */
    precompute_overlay(dsz);
    uint16_t *buf16 = (uint16_t *)canvas_buf;
    /* Dark gray overlay for light theme (RGB565: 0x3186 ~ #333333) */
    const uint16_t overlay_color = 0x3186;
    const uint32_t total_pixels = dsz * dsz;
    const uint32_t overlay_words = (total_pixels + 31u) / 32u;

    for (uint32_t word = 0; word < overlay_words; word++) {
        uint32_t bits = s_overlay_mask[word];
        if (bits == 0) continue;  /* Skip empty words (common case) */
        uint32_t base = word << 5;
        while (bits) {
            uint32_t bit = bits & (uint32_t)(-(int32_t)bits); /* lowest set bit */
            uint32_t idx = base + (uint32_t)__builtin_ctz(bits);
            if (idx < total_pixels) {
                buf16[idx] = overlay_color;
            }
            bits ^= bit;
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
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(COL_BTN_PAUSE), 0);
    } else {
        lv_label_set_text(view->label_play_pause_text, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(view->btn_play_pause, lv_color_hex(COL_BTN_PLAY), 0);
    }
}

void reconstruction_viewer_view_set_noise_state(reconstruction_viewer_view_t *view, int enabled)
{
    if (!view || !view->btn_noise || !view->label_noise_text) return;

    if (enabled) {
        lv_label_set_text(view->label_noise_text, "N:ON");
        lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(COL_SLIDER_IND), 0);
    } else {
        lv_label_set_text(view->label_noise_text, "N:OFF");
        lv_obj_set_style_bg_color(view->btn_noise, lv_color_hex(COL_BTN_SEC), 0);
    }
}

void reconstruction_viewer_view_set_noise_level(reconstruction_viewer_view_t *view, int32_t pct)
{
    if (!view || !view->label_noise_pct) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", (long)pct);
    lv_label_set_text(view->label_noise_pct, buf);
}

void reconstruction_viewer_view_set_algorithm(reconstruction_viewer_view_t *view, const char *name)
{
    if (!view || !view->label_algo) return;
    lv_label_set_text(view->label_algo, name ? name : "");
}

void reconstruction_viewer_view_set_display_size(reconstruction_viewer_view_t *view, uint32_t new_size)
{
    if (!view || !view->canvas) return;
    if (new_size == 0 || new_size > EIT_DISPLAY_SIZE_MAX) new_size = EIT_DISPLAY_SIZE;
    if (new_size == view->display_size) return;

    view->display_size = new_size;

    /* Recreate canvas at the new size (reuses SDRAM buffer) */
    lv_color_t *canvas_buf = (lv_color_t *)EIT_SDRAM_CANVAS_BUF_ADDR;
    lv_canvas_set_buffer(view->canvas, canvas_buf, new_size, new_size, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(view->canvas, lv_color_hex(COL_BG), LV_OPA_COVER);
    lv_obj_align(view->canvas, LV_ALIGN_CENTER, -30, 20);

    /* Force overlay recomputation */
    s_overlay_ready = 0;
}

void reconstruction_viewer_view_set_stream_state(reconstruction_viewer_view_t *view, int active)
{
    if (!view || !view->btn_stream || !view->label_stream_text) return;

    if (active) {
        lv_obj_set_style_bg_color(view->btn_stream, lv_color_hex(COL_BTN_PLAY), 0);
        lv_label_set_text(view->label_stream_text, LV_SYMBOL_WIFI " LIVE");
    } else {
        lv_obj_set_style_bg_color(view->btn_stream, lv_color_hex(COL_BTN_SEC), 0);
        lv_label_set_text(view->label_stream_text, LV_SYMBOL_WIFI " STREAM");
    }
}

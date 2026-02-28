#ifndef RECONSTRUCTION_VIEWER_VIEW_H
#define RECONSTRUCTION_VIEWER_VIEW_H

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*recon_view_on_return_cb_t)(void *ctx);
typedef void (*recon_view_on_save_cb_t)(void *ctx);
typedef void (*recon_view_on_play_pause_cb_t)(void *ctx);

typedef struct {
    void *ctx;
    recon_view_on_return_cb_t on_return;
    recon_view_on_save_cb_t on_save;
    recon_view_on_play_pause_cb_t on_play_pause;
} reconstruction_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *canvas;
    lv_obj_t *label_title;
    lv_obj_t *label_status;
    lv_obj_t *label_fps;
    lv_obj_t *btn_save;
    lv_obj_t *btn_play_pause;
    lv_obj_t *label_play_pause_text;

    reconstruction_view_bindings_t bindings;
} reconstruction_viewer_view_t;

void reconstruction_viewer_view_create(reconstruction_viewer_view_t *view, lv_obj_t *parent, const reconstruction_view_bindings_t *bindings);
void reconstruction_viewer_view_set_title(reconstruction_viewer_view_t *view, const char *filename);
void reconstruction_viewer_view_set_status(reconstruction_viewer_view_t *view, const char *text);
void reconstruction_viewer_view_set_save_enabled(reconstruction_viewer_view_t *view, int enabled);

/* Copies a DISPLAY_SIZE x DISPLAY_SIZE RGB565 buffer into the canvas and draws overlays (border + dots). */
void reconstruction_viewer_view_render_rgb565(reconstruction_viewer_view_t *view, const uint16_t *rgb565, uint32_t width, uint32_t height);

/* Update FPS display. fps_x10 = FPS * 10 (e.g. 125 means 12.5 FPS). */
void reconstruction_viewer_view_set_fps(reconstruction_viewer_view_t *view, uint32_t fps_x10);

/* Update play/pause button visual state (1 = playing, 0 = paused). */
void reconstruction_viewer_view_set_play_state(reconstruction_viewer_view_t *view, int playing);

#ifdef __cplusplus
}
#endif

#endif /* RECONSTRUCTION_VIEWER_VIEW_H */

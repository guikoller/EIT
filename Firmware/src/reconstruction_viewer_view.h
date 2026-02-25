#ifndef RECONSTRUCTION_VIEWER_VIEW_H
#define RECONSTRUCTION_VIEWER_VIEW_H

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*recon_view_on_return_cb_t)(void *ctx);
typedef void (*recon_view_on_save_cb_t)(void *ctx);

typedef struct {
    void *ctx;
    recon_view_on_return_cb_t on_return;
    recon_view_on_save_cb_t on_save;
} reconstruction_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *canvas;
    lv_obj_t *label_title;
    lv_obj_t *label_status;
    lv_obj_t *btn_save;

    reconstruction_view_bindings_t bindings;
} reconstruction_viewer_view_t;

void reconstruction_viewer_view_create(reconstruction_viewer_view_t *view, lv_obj_t *parent, const reconstruction_view_bindings_t *bindings);
void reconstruction_viewer_view_set_title(reconstruction_viewer_view_t *view, const char *filename);
void reconstruction_viewer_view_set_status(reconstruction_viewer_view_t *view, const char *text);
void reconstruction_viewer_view_set_save_enabled(reconstruction_viewer_view_t *view, int enabled);

/* Copies a DISPLAY_SIZE x DISPLAY_SIZE RGB565 buffer into the canvas and draws overlays (border + dots). */
void reconstruction_viewer_view_render_rgb565(reconstruction_viewer_view_t *view, const uint16_t *rgb565, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* RECONSTRUCTION_VIEWER_VIEW_H */

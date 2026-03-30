#ifndef HOME_VIEW_H
#define HOME_VIEW_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*home_menu_cb_t)(void *ctx);

typedef struct {
    void           *ctx;
    home_menu_cb_t  on_start;
    home_menu_cb_t  on_settings;
} home_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *img_logo;
    lv_obj_t *label_title;
    lv_obj_t *label_subtitle;
    lv_obj_t *btn_start;
    lv_obj_t *btn_settings;
    lv_obj_t *btn_about;       /* unused, kept for ABI compat */
    lv_obj_t *about_overlay;   /* unused, kept for ABI compat */

    home_view_bindings_t bindings;
} home_view_t;

void home_view_create(home_view_t *view, lv_obj_t *parent,
                      const home_view_bindings_t *bindings);

#ifdef __cplusplus
}
#endif

#endif /* HOME_VIEW_H */

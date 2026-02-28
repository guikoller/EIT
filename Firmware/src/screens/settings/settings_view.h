#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include "lvgl/lvgl.h"
#include "eit_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*settings_cb_t)(void *ctx);
typedef void (*settings_algo_cb_t)(void *ctx, eit_algorithm_t algo);
typedef void (*settings_imgsize_cb_t)(void *ctx, uint16_t size);
typedef void (*settings_bool_cb_t)(void *ctx, uint8_t value);

typedef struct {
    void               *ctx;
    settings_cb_t       on_back;
    settings_algo_cb_t  on_algorithm;
    settings_imgsize_cb_t on_image_size;
    settings_bool_cb_t  on_show_data_table;
} settings_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *label_title;

    /* Algorithm dropdown */
    lv_obj_t *dd_algorithm;

    /* Image-size dropdown */
    lv_obj_t *dd_image_size;

    /* Show data-table checkbox */
    lv_obj_t *cb_data_table;

    /* Back button */
    lv_obj_t *btn_back;

    settings_view_bindings_t bindings;
} settings_view_t;

void settings_view_create(settings_view_t *view, lv_obj_t *parent,
                          const settings_view_bindings_t *bindings);

/** Push current values into the widgets (call after create). */
void settings_view_set_values(settings_view_t *view,
                              eit_algorithm_t algo,
                              uint16_t image_size,
                              uint8_t show_data_table);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_VIEW_H */

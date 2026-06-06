#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include "lvgl/lvgl.h"
#include "eit_config.h"
#include "screens/common/left_menu.h"

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
    settings_cb_t       on_nav_home;
    settings_cb_t       on_nav_eit;
    settings_cb_t       on_nav_settings;
    settings_algo_cb_t  on_algorithm;
    settings_imgsize_cb_t on_image_size;
    settings_bool_cb_t  on_show_data_table;
    settings_cb_t       on_calibrate;
    settings_cb_t       on_batch;
    settings_cb_t       on_wifi;
    settings_cb_t       on_serial_monitor;
    settings_cb_t       on_send_exam;
} settings_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *content;
    left_menu_t menu;
    lv_obj_t *label_title;

    /* Algorithm dropdown */
    lv_obj_t *dd_algorithm;

    /* Image-size dropdown */
    lv_obj_t *dd_image_size;

    /* Show data-table switch */
    lv_obj_t *sw_data_table;

    /* Calibrate button + status */
    lv_obj_t *btn_calibrate;
    lv_obj_t *label_calib_status;

    /* Batch process button + status */
    lv_obj_t *btn_batch;
    lv_obj_t *label_batch_status;

    /* WiFi settings button */
    lv_obj_t *btn_wifi;

    /* Serial monitor button */
    lv_obj_t *btn_serial_monitor;

    /* Send exam button */
    lv_obj_t *btn_send_exam;

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

/** Update the calibration status text shown below the card. */
void settings_view_set_calib_status(settings_view_t *view, const char *text);

/** Enable / disable the calibrate button. */
void settings_view_set_calib_enabled(settings_view_t *view, int enabled);

/** Update the batch-process status text. */
void settings_view_set_batch_status(settings_view_t *view, const char *text);

/** Enable / disable the batch-process button. */
void settings_view_set_batch_enabled(settings_view_t *view, int enabled);

/** Enable / disable the send-exam button. */
void settings_view_set_send_exam_enabled(settings_view_t *view, int enabled);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_VIEW_H */

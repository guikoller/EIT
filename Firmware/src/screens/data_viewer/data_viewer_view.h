#ifndef DATA_VIEWER_VIEW_H
#define DATA_VIEWER_VIEW_H

#include "lvgl.h"
#include "screens/common/left_menu.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*data_viewer_on_return_cb_t)(void *ctx);
typedef void (*data_viewer_on_run_cb_t)(void *ctx);
typedef void (*data_viewer_on_tab_changed_cb_t)(void *ctx, uint32_t tab_id);

typedef struct {
    void *ctx;
    data_viewer_on_return_cb_t on_return;
    data_viewer_on_run_cb_t on_run;
    data_viewer_on_tab_changed_cb_t on_tab_changed;
    data_viewer_on_return_cb_t on_nav_home;
    data_viewer_on_return_cb_t on_nav_eit;
    data_viewer_on_return_cb_t on_nav_settings;
} data_viewer_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *content;
    left_menu_t menu;
    lv_obj_t *tabview;
    lv_obj_t *label_title;
    lv_obj_t *btn_return;
    lv_obj_t *btn_run;

    lv_obj_t *table_curr;
    lv_obj_t *table_uel;
    lv_obj_t *table_meas;

    uint16_t n_meas;
    uint16_t n_inj;
    uint16_t curr_pattern_rows;
    uint16_t meas_pattern_rows;

    uint8_t curr_populated;
    uint8_t uel_populated;
    uint8_t meas_populated;

    data_viewer_view_bindings_t bindings;
} data_viewer_view_t;

void data_viewer_view_create(data_viewer_view_t *view, lv_obj_t *parent, const data_viewer_view_bindings_t *bindings);
void data_viewer_view_set_title(data_viewer_view_t *view, const char *filename);
void data_viewer_view_set_dataset_meta(data_viewer_view_t *view,
                                      uint16_t n_meas,
                                      uint16_t n_inj,
                                      uint16_t curr_pattern_rows,
                                      uint16_t meas_pattern_rows);

void data_viewer_view_populate_current(data_viewer_view_t *view, const int8_t *curr_pattern);
void data_viewer_view_populate_uel(data_viewer_view_t *view, const float *uel_data);
void data_viewer_view_populate_meas(data_viewer_view_t *view, const int8_t *meas_pattern);

#ifdef __cplusplus
}
#endif

#endif /* DATA_VIEWER_VIEW_H */

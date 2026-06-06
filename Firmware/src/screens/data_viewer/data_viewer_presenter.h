#ifndef DATA_VIEWER_PRESENTER_H
#define DATA_VIEWER_PRESENTER_H

#include "data_viewer_view.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    data_viewer_view_t *view;

    char loaded_filename[128];

    float *uel_data;
    int8_t *curr_pattern;
    int8_t *meas_pattern;

    uint16_t n_meas;
    uint16_t n_inj;
    uint16_t curr_pattern_rows;
    uint16_t meas_pattern_rows;

    float **uel_2d;
    uint16_t uel_2d_rows;
} data_viewer_presenter_t;

void data_viewer_presenter_init(data_viewer_presenter_t *presenter, data_viewer_view_t *view);
void data_viewer_presenter_on_create(data_viewer_presenter_t *presenter, const char *filename);

void data_viewer_presenter_on_return(void *ctx);
void data_viewer_presenter_on_run(void *ctx);
void data_viewer_presenter_on_tab_changed(void *ctx, uint32_t tab_id);
void data_viewer_presenter_on_nav_home(void *ctx);
void data_viewer_presenter_on_nav_eit(void *ctx);
void data_viewer_presenter_on_nav_settings(void *ctx);

float **data_viewer_presenter_get_uel(data_viewer_presenter_t *presenter, uint16_t *out_n_meas, uint16_t *out_n_inj);

#ifdef __cplusplus
}
#endif

#endif /* DATA_VIEWER_PRESENTER_H */

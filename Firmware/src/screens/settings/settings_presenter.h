#ifndef SETTINGS_PRESENTER_H
#define SETTINGS_PRESENTER_H

#include "settings_view.h"
#include "app/app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    settings_view_t *view;

    lv_timer_t *calib_timer;
    uint8_t     calib_start_pending;

    lv_timer_t *batch_timer;
    uint8_t     batch_start_pending;
} settings_presenter_t;

void settings_presenter_init(settings_presenter_t *p, settings_view_t *v);
void settings_presenter_on_create(settings_presenter_t *p);

/* View callbacks */
void settings_presenter_on_back(void *ctx);
void settings_presenter_on_algorithm(void *ctx, eit_algorithm_t algo);
void settings_presenter_on_image_size(void *ctx, uint16_t size);
void settings_presenter_on_show_data_table(void *ctx, uint8_t value);
void settings_presenter_on_calibrate(void *ctx);
void settings_presenter_on_batch(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_PRESENTER_H */

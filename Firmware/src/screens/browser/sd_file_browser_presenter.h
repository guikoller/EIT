#ifndef SD_FILE_BROWSER_PRESENTER_H
#define SD_FILE_BROWSER_PRESENTER_H

#include "sd_file_browser_view.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    sd_file_browser_view_t *view;

    lv_timer_t *calib_timer;
    uint8_t calib_start_pending;
} sd_file_browser_presenter_t;

void sd_file_browser_presenter_init(sd_file_browser_presenter_t *presenter, sd_file_browser_view_t *view);
void sd_file_browser_presenter_on_create(sd_file_browser_presenter_t *presenter);

void sd_file_browser_presenter_on_load(void *ctx, const char *filename);
void sd_file_browser_presenter_on_compute_sens_matrix(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SD_FILE_BROWSER_PRESENTER_H */

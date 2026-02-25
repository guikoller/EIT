#ifndef RECONSTRUCTION_VIEWER_PRESENTER_H
#define RECONSTRUCTION_VIEWER_PRESENTER_H

#include "reconstruction_viewer_view.h"

#include "lbp_reconstruction.h"

#include "ff.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    reconstruction_viewer_view_t *view;

    char current_filename[128];
    ReconstructionResult *recon_result;

    lv_timer_t *save_timer;
    FIL save_file;
    char save_path[64];
    uint32_t save_row;
    uint32_t save_bytes_written;
    uint32_t save_bytes_total;
    FRESULT save_last_res;
} reconstruction_viewer_presenter_t;

void reconstruction_viewer_presenter_init(reconstruction_viewer_presenter_t *presenter, reconstruction_viewer_view_t *view);
void reconstruction_viewer_presenter_on_create(reconstruction_viewer_presenter_t *presenter, const char *filename);

void reconstruction_viewer_presenter_on_return(void *ctx);
void reconstruction_viewer_presenter_on_save(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RECONSTRUCTION_VIEWER_PRESENTER_H */

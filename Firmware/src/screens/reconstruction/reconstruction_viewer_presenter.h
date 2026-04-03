#ifndef RECONSTRUCTION_VIEWER_PRESENTER_H
#define RECONSTRUCTION_VIEWER_PRESENTER_H

#include "reconstruction_viewer_view.h"

#include "algorithms/lbp_reconstruction.h"
#include "services/eit_acquisition.h"

#include "ff.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    reconstruction_viewer_view_t *view;

    char current_filename[128];
    ReconstructionResult *recon_result;

    /* Data acquisition (hardware abstraction layer) */
    eit_acq_backend_t *acq_backend;
    eit_frame_t ref_frame;      /* Stored reference for LBP */
    uint16_t acq_n_meas;
    uint16_t acq_n_inj;

    /* Continuous display loop */
    lv_timer_t *acq_timer;
    uint32_t frame_count;
    uint32_t fps_tick_start;
    int is_playing;

    /* Noise injection (UI state — forwarded to backend) */
    int noise_enabled;
    int32_t noise_level_pct;

    lv_timer_t *save_timer;
    FIL save_file;
    char save_path[64];
    uint32_t save_row;
    uint32_t save_bytes_written;
    uint32_t save_bytes_total;
    FRESULT save_last_res;

    /* JSON recording state */
    char recorded_json_path[64];
    int has_recorded_data;
    int send_pending;
    char send_reply[256];
} reconstruction_viewer_presenter_t;

void reconstruction_viewer_presenter_init(reconstruction_viewer_presenter_t *presenter, reconstruction_viewer_view_t *view);
void reconstruction_viewer_presenter_on_create(reconstruction_viewer_presenter_t *presenter, const char *filename);

void reconstruction_viewer_presenter_on_return(void *ctx);
void reconstruction_viewer_presenter_on_save(void *ctx);
void reconstruction_viewer_presenter_on_record(void *ctx);
void reconstruction_viewer_presenter_on_send(void *ctx);
void reconstruction_viewer_presenter_on_play_pause(void *ctx);
void reconstruction_viewer_presenter_on_noise_toggle(void *ctx);
void reconstruction_viewer_presenter_on_noise_level(void *ctx, int32_t level);
void reconstruction_viewer_presenter_on_nav_home(void *ctx);
void reconstruction_viewer_presenter_on_nav_eit(void *ctx);
void reconstruction_viewer_presenter_on_nav_settings(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RECONSTRUCTION_VIEWER_PRESENTER_H */

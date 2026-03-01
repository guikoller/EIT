#include "settings_presenter.h"

#include "app/app_coordinator.h"
#include "services/calibration.h"
#include "services/storage_service.h"

#include <string.h>
#include <stdio.h>

/* ---- Calibration helpers (moved from browser presenter) ---- */

static const char *calib_stage_text(calib_stage_t st)
{
    switch (st) {
        case CALIB_STAGE_OPEN_DATASET:      return "OPENING";
        case CALIB_STAGE_READ_HEADER:       return "READING";
        case CALIB_STAGE_READ_PATTERNS:     return "PATTERNS";
        case CALIB_STAGE_PRECOMPUTE_FIELDS: return "GENERATING";
        case CALIB_STAGE_OPEN_OUTPUT:       return "OPEN OUT";
        case CALIB_STAGE_WRITE_HEADER:      return "WRITE HDR";
        case CALIB_STAGE_WRITE_DATA:        return "WRITING";
        case CALIB_STAGE_CLOSE_FILE:        return "CLOSING";
        default: return "";
    }
}

static void calib_timer_cb(lv_timer_t *t)
{
    settings_presenter_t *p = (settings_presenter_t *)lv_timer_get_user_data(t);
    if (!p || !p->view) return;
    if (!p->view->label_calib_status ||
        !lv_obj_is_valid(p->view->label_calib_status)) {
        if (p->calib_timer == t)
            p->calib_timer = NULL;
        lv_timer_del(t);
        return;
    }

    calib_progress_t prog;
    calib_status_t st = sensitivity_matrix_step(&prog);

    if (st == CALIB_STATUS_RUNNING) {
        uint32_t pct = 0;
        if (prog.bytes_total > 0)
            pct = (uint32_t)((prog.bytes_written * 100u) / prog.bytes_total);

        char msg[64];
        snprintf(msg, sizeof(msg), "SENS %s %lu%%",
                 calib_stage_text(prog.stage), (unsigned long)pct);
        settings_view_set_calib_status(p->view, msg);
        return;
    }

    if (st == CALIB_STATUS_DONE) {
        settings_view_set_calib_status(p->view, "SENS DONE");
        settings_view_set_calib_enabled(p->view, 1);
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "SENS ERR %s (r%u)",
                 calib_stage_text(prog.stage), (unsigned)prog.fresult);
        settings_view_set_calib_status(p->view, msg);
        settings_view_set_calib_enabled(p->view, 1);
    }

    if (p->calib_timer) {
        lv_timer_del(p->calib_timer);
        p->calib_timer = NULL;
    }
}

static void compute_sens_matrix_start_async_cb(void *user_data)
{
    settings_presenter_t *p = (settings_presenter_t *)user_data;
    if (!p || !p->view) return;

    p->calib_start_pending = 0u;

    if (!storage_service_is_mounted()) {
        settings_view_set_calib_status(p->view, "SENS ERR NO SD");
        settings_view_set_calib_enabled(p->view, 1);
        return;
    }

    if (!sensitivity_matrix_begin_from_dataset("datamat_1_0.bin",
                                               "sensitivity_matrix.bin")) {
        settings_view_set_calib_status(p->view, "SENS ERR START");
        settings_view_set_calib_enabled(p->view, 1);
        return;
    }

    if (p->calib_timer) {
        lv_timer_del(p->calib_timer);
        p->calib_timer = NULL;
    }
    p->calib_timer = lv_timer_create(calib_timer_cb, 25, p);
}

/* ---- Presenter lifecycle ---- */

void settings_presenter_init(settings_presenter_t *p, settings_view_t *v)
{
    if (!p) return;
    if (p->calib_timer) {
        lv_timer_del(p->calib_timer);
        p->calib_timer = NULL;
    }
    memset(p, 0, sizeof(*p));
    p->view = v;
}

void settings_presenter_on_create(settings_presenter_t *p)
{
    if (!p || !p->view) return;

    /* Read current settings from the coordinator and push into the view */
    const app_state_t *st = app_coordinator_get_state();
    settings_view_set_values(p->view,
                             st->settings.algorithm,
                             st->settings.image_size,
                             st->settings.show_data_table);
}

void settings_presenter_on_back(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void settings_presenter_on_algorithm(void *ctx, eit_algorithm_t algo)
{
    (void)ctx;
    /* Write directly — settings are shared via coordinator state */
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.algorithm = algo;
}

void settings_presenter_on_image_size(void *ctx, uint16_t size)
{
    (void)ctx;
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.image_size = size;
}

void settings_presenter_on_show_data_table(void *ctx, uint8_t value)
{
    (void)ctx;
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.show_data_table = value;
}

void settings_presenter_on_calibrate(void *ctx)
{
    settings_presenter_t *p = (settings_presenter_t *)ctx;
    if (!p || !p->view) return;

    /* Ignore if already running */
    if (p->calib_timer || p->calib_start_pending) return;

    settings_view_set_calib_status(p->view, "SENS GENERATING...");
    settings_view_set_calib_enabled(p->view, 0);

    p->calib_start_pending = 1u;
    lv_async_call(compute_sens_matrix_start_async_cb, p);
}

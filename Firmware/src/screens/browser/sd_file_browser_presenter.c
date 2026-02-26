#include "sd_file_browser_presenter.h"

#include "app/app_coordinator.h"
#include "services/calibration.h"
#include "services/storage_service.h"

#include <string.h>
#include <stdio.h>

static const char *calib_stage_text(calib_stage_t st)
{
    switch (st) {
        case CALIB_STAGE_OPEN_DATASET: return "OPENING";
        case CALIB_STAGE_READ_HEADER: return "READING";
        case CALIB_STAGE_READ_PATTERNS: return "PATTERNS";
        case CALIB_STAGE_PRECOMPUTE_FIELDS: return "GENERATING";
        case CALIB_STAGE_OPEN_OUTPUT: return "OPEN OUT";
        case CALIB_STAGE_WRITE_HEADER: return "WRITE HDR";
        case CALIB_STAGE_WRITE_DATA: return "WRITING";
        case CALIB_STAGE_CLOSE_FILE: return "CLOSING";
        default: return "";
    }
}

static void calib_timer_cb(lv_timer_t *t)
{
    sd_file_browser_presenter_t *p = (sd_file_browser_presenter_t *)lv_timer_get_user_data(t);
    if (!p || !p->view) return;
    if (!p->view->label_status || !lv_obj_is_valid(p->view->label_status)) {
        if (p->calib_timer == t) {
            p->calib_timer = NULL;
        }
        lv_timer_del(t);
        return;
    }

    calib_progress_t prog;
    calib_status_t st = sensitivity_matrix_step(&prog);

    if (st == CALIB_STATUS_RUNNING) {
        uint32_t pct = 0;
        if (prog.bytes_total > 0) {
            pct = (uint32_t)((prog.bytes_written * 100u) / prog.bytes_total);
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "SENS %s %lu%%", calib_stage_text(prog.stage), (unsigned long)pct);
        sd_file_browser_view_set_status(p->view, msg);
        return;
    }

    if (st == CALIB_STATUS_DONE) {
        sd_file_browser_view_set_status(p->view, "SENS DONE");
        sd_file_browser_view_set_enabled(p->view, 1);
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "SENS ERR %s (r%u)", calib_stage_text(prog.stage), (unsigned)prog.fresult);
        sd_file_browser_view_set_status(p->view, msg);
        sd_file_browser_view_set_enabled(p->view, 1);
    }

    if (p->calib_timer) {
        lv_timer_del(p->calib_timer);
        p->calib_timer = NULL;
    }
}

static void compute_sens_matrix_start_async_cb(void *user_data)
{
    sd_file_browser_presenter_t *p = (sd_file_browser_presenter_t *)user_data;
    if (!p || !p->view) return;

    if (!p->view->label_status || !lv_obj_is_valid(p->view->label_status)) {
        return;
    }

    p->calib_start_pending = 0u;

    if (!storage_service_is_mounted()) {
        sd_file_browser_view_set_status(p->view, "SENS ERR NO SD");
        sd_file_browser_view_set_enabled(p->view, 1);
        return;
    }

    if (!sensitivity_matrix_begin_from_dataset("datamat_1_0.bin", "sensitivity_matrix.bin")) {
        sd_file_browser_view_set_status(p->view, "SENS ERR START");
        sd_file_browser_view_set_enabled(p->view, 1);
        return;
    }

    if (p->calib_timer) {
        lv_timer_del(p->calib_timer);
        p->calib_timer = NULL;
    }
    p->calib_timer = lv_timer_create(calib_timer_cb, 25, p);
}

void sd_file_browser_presenter_init(sd_file_browser_presenter_t *presenter, sd_file_browser_view_t *view)
{
    if (!presenter) return;

    if (presenter->calib_timer) {
        lv_timer_del(presenter->calib_timer);
        presenter->calib_timer = NULL;
    }
    memset(presenter, 0, sizeof(*presenter));
    presenter->view = view;
}

void sd_file_browser_presenter_on_create(sd_file_browser_presenter_t *presenter)
{
    if (!presenter || !presenter->view) return;

    sd_file_browser_view_set_status(presenter->view, "READY");
    sd_file_browser_view_set_enabled(presenter->view, 1);

    storage_file_entry_t tmp[50];
    memset(tmp, 0, sizeof(tmp));

    sd_browser_file_entry_t entries[50];
    memset(entries, 0, sizeof(entries));

    int count = 0;
    if (storage_service_scan_root(tmp, 50, &count) != 0) {
        count = 0;
    }

    for (int i = 0; i < count && i < 50; i++) {
        strncpy(entries[i].filename, tmp[i].filename, sizeof(entries[i].filename) - 1);
        entries[i].filename[sizeof(entries[i].filename) - 1] = '\0';
        entries[i].size = tmp[i].size;
        entries[i].is_valid = tmp[i].is_valid;
    }

    sd_file_browser_view_set_files(presenter->view, entries, count);
}

void sd_file_browser_presenter_on_load(void *ctx, const char *filename)
{
    sd_file_browser_presenter_t *p = (sd_file_browser_presenter_t *)ctx;
    (void)p;
    if (!filename || filename[0] == '\0') return;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_DATA_VIEWER;
    strncpy(evt.data.nav.filename, filename, sizeof(evt.data.nav.filename) - 1);
    evt.data.nav.filename[sizeof(evt.data.nav.filename) - 1] = '\0';
    app_coordinator_post_event(&evt);
}

void sd_file_browser_presenter_on_compute_sens_matrix(void *ctx)
{
    sd_file_browser_presenter_t *p = (sd_file_browser_presenter_t *)ctx;
    if (!p || !p->view) return;

    if (p->calib_timer || p->calib_start_pending) return;

    sd_file_browser_view_set_status(p->view, "SENS GENERATING...");
    sd_file_browser_view_set_enabled(p->view, 0);

    p->calib_start_pending = 1u;
    lv_async_call(compute_sens_matrix_start_async_cb, p);
}

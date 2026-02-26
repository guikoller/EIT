#include "data_viewer_presenter.h"

#include "app/app_coordinator.h"
#include "services/dataset_service.h"

#include <string.h>
#include <stdlib.h>

static void presenter_free_loaded_data(data_viewer_presenter_t *p)
{
    if (!p) return;

    if (p->uel_2d) {
        free(p->uel_2d);
        p->uel_2d = NULL;
        p->uel_2d_rows = 0u;
    }

    if (p->uel_data) {
        free(p->uel_data);
        p->uel_data = NULL;
    }
    if (p->curr_pattern) {
        free(p->curr_pattern);
        p->curr_pattern = NULL;
    }
    if (p->meas_pattern) {
        free(p->meas_pattern);
        p->meas_pattern = NULL;
    }

    p->n_meas = 0u;
    p->n_inj = 0u;
    p->curr_pattern_rows = 0u;
    p->meas_pattern_rows = 0u;
}

static int presenter_load_binary_file(data_viewer_presenter_t *p, const char *filename)
{
    if (!p || !filename || filename[0] == '\0') return -1;

    /* Drop any previously loaded dataset buffers first.
     * Important: presenter_free_loaded_data() also resets meta fields.
     */
    presenter_free_loaded_data(p);

    dataset_t ds;
    int rc = dataset_service_load_full(filename, &ds);
    if (rc != 0) {
        dataset_service_free(&ds);
        return rc;
    }

    p->n_meas = ds.header.n_meas;
    p->n_inj = ds.header.n_inj;
    p->curr_pattern_rows = ds.header.curr_pattern_rows;
    p->meas_pattern_rows = ds.header.meas_pattern_rows;

    p->uel_data = ds.uel_1d;
    p->curr_pattern = ds.curr_pattern;
    p->meas_pattern = ds.meas_pattern;

    ds.uel_1d = NULL;
    ds.curr_pattern = NULL;
    ds.meas_pattern = NULL;
    dataset_service_free(&ds);

    return 0;
}

static void run_async_cb(void *user_data)
{
    data_viewer_presenter_t *p = (data_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_free_loaded_data(p);

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_RECON_VIEWER;
    strncpy(evt.data.nav.filename, p->loaded_filename, sizeof(evt.data.nav.filename) - 1);
    evt.data.nav.filename[sizeof(evt.data.nav.filename) - 1] = '\0';
    app_coordinator_post_event(&evt);
}

static void return_async_cb(void *user_data)
{
    data_viewer_presenter_t *p = (data_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_free_loaded_data(p);

    app_event_t evt;
    evt.type = APP_EVENT_BACK;
    app_coordinator_post_event(&evt);
}

void data_viewer_presenter_init(data_viewer_presenter_t *presenter, data_viewer_view_t *view)
{
    if (!presenter) return;

    presenter_free_loaded_data(presenter);
    memset(presenter, 0, sizeof(*presenter));
    presenter->view = view;
}

void data_viewer_presenter_on_create(data_viewer_presenter_t *presenter, const char *filename)
{
    if (!presenter || !presenter->view || !filename) return;

    strncpy(presenter->loaded_filename, filename, sizeof(presenter->loaded_filename) - 1);
    presenter->loaded_filename[sizeof(presenter->loaded_filename) - 1] = '\0';

    int rc = presenter_load_binary_file(presenter, filename);
    if (rc != 0) {
        presenter_free_loaded_data(presenter);
        app_event_t evt;
        evt.type = APP_EVENT_OPEN_BROWSER;
        app_coordinator_post_event(&evt);
        return;
    }

    data_viewer_view_set_dataset_meta(presenter->view,
                                     presenter->n_meas,
                                     presenter->n_inj,
                                     presenter->curr_pattern_rows,
                                     presenter->meas_pattern_rows);
    data_viewer_view_set_title(presenter->view, filename);

    data_viewer_view_populate_current(presenter->view, presenter->curr_pattern);
}

void data_viewer_presenter_on_return(void *ctx)
{
    data_viewer_presenter_t *p = (data_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(return_async_cb, p);
}

void data_viewer_presenter_on_run(void *ctx)
{
    data_viewer_presenter_t *p = (data_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(run_async_cb, p);
}

void data_viewer_presenter_on_tab_changed(void *ctx, uint32_t tab_id)
{
    data_viewer_presenter_t *p = (data_viewer_presenter_t *)ctx;
    if (!p || !p->view) return;

    switch (tab_id) {
        case 0:
            data_viewer_view_populate_current(p->view, p->curr_pattern);
            break;
        case 1:
            data_viewer_view_populate_uel(p->view, p->uel_data);
            break;
        case 2:
            data_viewer_view_populate_meas(p->view, p->meas_pattern);
            break;
        default:
            break;
    }
}

float **data_viewer_presenter_get_uel(data_viewer_presenter_t *presenter, uint16_t *out_n_meas, uint16_t *out_n_inj)
{
    if (!presenter || !presenter->uel_data || !out_n_meas || !out_n_inj) {
        return NULL;
    }

    *out_n_meas = presenter->n_meas;
    *out_n_inj = presenter->n_inj;

    if (presenter->uel_2d && presenter->uel_2d_rows != presenter->n_meas) {
        free(presenter->uel_2d);
        presenter->uel_2d = NULL;
        presenter->uel_2d_rows = 0u;
    }

    if (!presenter->uel_2d) {
        presenter->uel_2d = (float **)malloc(presenter->n_meas * sizeof(float *));
        if (!presenter->uel_2d) return NULL;
        presenter->uel_2d_rows = presenter->n_meas;
    }

    for (uint16_t i = 0; i < presenter->n_meas; i++) {
        presenter->uel_2d[i] = &presenter->uel_data[i * presenter->n_inj];
    }

    return presenter->uel_2d;
}

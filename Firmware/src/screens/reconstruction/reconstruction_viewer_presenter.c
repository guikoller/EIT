#include "reconstruction_viewer_presenter.h"

#include "app/app_coordinator.h"
#include "services/dataset_service.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define DISPLAY_SIZE 288

static void presenter_cleanup(reconstruction_viewer_presenter_t *p)
{
    if (!p) return;

    if (p->save_timer) {
        lv_timer_del(p->save_timer);
        p->save_timer = NULL;
    }

    if (p->recon_result) {
        lbp_free_result(p->recon_result);
        p->recon_result = NULL;
    }
}

static void recon_return_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_cleanup(p);

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

static int make_unique_bmp_path(char *out, size_t outsz)
{
    FILINFO fno;
    for (uint32_t i = 0; i < 1000; i++) {
        snprintf(out, outsz, "0:/recon_%03lu.bmp", (unsigned long)i);
        if (f_stat(out, &fno) != FR_OK) {
            return 1;
        }
    }
    return 0;
}

static void write_u16_le(uint8_t *dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void write_i32_le(uint8_t *dst, int32_t v)
{
    write_u32_le(dst, (uint32_t)v);
}

static void save_timer_cb(lv_timer_t *t)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)lv_timer_get_user_data(t);
    if (!p || !p->view) return;

    const uint16_t *src_buf = NULL;
    if (p->recon_result && p->recon_result->success && p->recon_result->color_buffer) {
        src_buf = p->recon_result->color_buffer;
    }

    if (!src_buf) {
        if (p->save_timer) {
            lv_timer_del(p->save_timer);
            p->save_timer = NULL;
        }
        f_close(&p->save_file);
        reconstruction_viewer_view_set_save_enabled(p->view, 1);
        reconstruction_viewer_view_set_status(p->view, "SAVE ERROR: no data");
        return;
    }

    static uint8_t rowbuf[DISPLAY_SIZE * 3u];
    const uint32_t width = DISPLAY_SIZE;
    const uint32_t height = DISPLAY_SIZE;
    const uint32_t rows_per_tick = 2u;

    for (uint32_t r = 0; r < rows_per_tick && p->save_row < height; r++) {
        const uint16_t *src = src_buf + (p->save_row * width);
        uint8_t *dst = rowbuf;

        for (uint32_t x = 0; x < width; x++) {
            uint16_t pix = src[x];
            uint8_t r5 = (uint8_t)((pix >> 11) & 0x1Fu);
            uint8_t g6 = (uint8_t)((pix >> 5) & 0x3Fu);
            uint8_t b5 = (uint8_t)(pix & 0x1Fu);

            uint8_t rr = (uint8_t)((r5 << 3) | (r5 >> 2));
            uint8_t gg = (uint8_t)((g6 << 2) | (g6 >> 4));
            uint8_t bb = (uint8_t)((b5 << 3) | (b5 >> 2));

            *dst++ = bb;
            *dst++ = gg;
            *dst++ = rr;
        }

        UINT bw = 0;
        p->save_last_res = f_write(&p->save_file, rowbuf, (UINT)(width * 3u), &bw);
        if (p->save_last_res != FR_OK || bw != (UINT)(width * 3u)) {
            if (p->save_timer) {
                lv_timer_del(p->save_timer);
                p->save_timer = NULL;
            }
            f_close(&p->save_file);
            reconstruction_viewer_view_set_save_enabled(p->view, 1);
            char err[64];
            snprintf(err, sizeof(err), "SAVE DATA ERR r%u", (unsigned)p->save_last_res);
            reconstruction_viewer_view_set_status(p->view, err);
            return;
        }

        p->save_bytes_written += bw;
        p->save_row++;
    }

    uint32_t pct = (p->save_bytes_total > 0) ? (p->save_bytes_written * 100u) / p->save_bytes_total : 0;
    if (pct > 100u) pct = 100u;
    char st[64];
    snprintf(st, sizeof(st), "Saving %lu%%...", (unsigned long)pct);
    reconstruction_viewer_view_set_status(p->view, st);

    if (p->save_row >= height) {
        f_close(&p->save_file);
        if (p->save_timer) {
            lv_timer_del(p->save_timer);
            p->save_timer = NULL;
        }
        reconstruction_viewer_view_set_save_enabled(p->view, 1);

        char done[96];
        const char *fn = p->save_path;
        if (strncmp(p->save_path, "0:/", 3) == 0) fn = &p->save_path[3];
        snprintf(done, sizeof(done), "Saved %s", fn);
        reconstruction_viewer_view_set_status(p->view, done);
    }
}

static void save_start_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p || !p->view) return;

    if (!p->recon_result || !p->recon_result->success || !p->recon_result->color_buffer) {
        reconstruction_viewer_view_set_status(p->view, "SAVE ERROR: no data");
        return;
    }

    if (!make_unique_bmp_path(p->save_path, sizeof(p->save_path))) {
        reconstruction_viewer_view_set_status(p->view, "SAVE ERROR: name");
        return;
    }

    p->save_last_res = f_open(&p->save_file, p->save_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (p->save_last_res != FR_OK) {
        char err[64];
        snprintf(err, sizeof(err), "SAVE OPEN ERR r%u", (unsigned)p->save_last_res);
        reconstruction_viewer_view_set_status(p->view, err);
        return;
    }

    const uint32_t width = DISPLAY_SIZE;
    const uint32_t height = DISPLAY_SIZE;
    const uint32_t row_bytes = width * 3u;
    const uint32_t pixel_bytes = row_bytes * height;
    const uint32_t file_size = 54u + pixel_bytes;

    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));

    hdr[0] = 'B';
    hdr[1] = 'M';
    write_u32_le(&hdr[2], file_size);
    write_u32_le(&hdr[10], 54u);

    write_u32_le(&hdr[14], 40u);
    write_i32_le(&hdr[18], (int32_t)width);
    write_i32_le(&hdr[22], -(int32_t)height);
    write_u16_le(&hdr[26], 1u);
    write_u16_le(&hdr[28], 24u);
    write_u32_le(&hdr[34], pixel_bytes);

    UINT bw = 0;
    p->save_last_res = f_write(&p->save_file, hdr, sizeof(hdr), &bw);
    if (p->save_last_res != FR_OK || bw != sizeof(hdr)) {
        f_close(&p->save_file);
        char err[64];
        snprintf(err, sizeof(err), "SAVE HDR ERR r%u", (unsigned)p->save_last_res);
        reconstruction_viewer_view_set_status(p->view, err);
        return;
    }

    p->save_row = 0u;
    p->save_bytes_written = sizeof(hdr);
    p->save_bytes_total = file_size;

    reconstruction_viewer_view_set_save_enabled(p->view, 0);
    reconstruction_viewer_view_set_status(p->view, "Saving 0%...");

    if (p->save_timer) {
        lv_timer_del(p->save_timer);
        p->save_timer = NULL;
    }
    p->save_timer = lv_timer_create(save_timer_cb, 10, p);
}

void reconstruction_viewer_presenter_init(reconstruction_viewer_presenter_t *presenter, reconstruction_viewer_view_t *view)
{
    if (!presenter) return;

    presenter_cleanup(presenter);
    memset(presenter, 0, sizeof(*presenter));

    presenter->view = view;
    presenter->save_last_res = FR_OK;
}

void reconstruction_viewer_presenter_on_create(reconstruction_viewer_presenter_t *presenter, const char *filename)
{
    if (!presenter || !presenter->view || !filename) return;

    strncpy(presenter->current_filename, filename, sizeof(presenter->current_filename) - 1);
    presenter->current_filename[sizeof(presenter->current_filename) - 1] = '\0';

    reconstruction_viewer_view_set_title(presenter->view, filename);
    reconstruction_viewer_view_set_save_enabled(presenter->view, 0);
    reconstruction_viewer_view_set_status(presenter->view, "Loading reference data...");

    uint16_t ref_n_meas = 0, ref_n_inj = 0;
    float **ref_uel = dataset_service_load_uel_2d("datamat_1_0.bin", &ref_n_meas, &ref_n_inj);
    if (!ref_uel) {
        reconstruction_viewer_view_set_status(presenter->view, "ERROR: Reference file datamat_1_0.bin not found on SD card");
        return;
    }

    reconstruction_viewer_view_set_status(presenter->view, "Loading target data...");

    uint16_t tgt_n_meas = 0, tgt_n_inj = 0;
    float **tgt_uel = dataset_service_load_uel_2d(filename, &tgt_n_meas, &tgt_n_inj);
    if (!tgt_uel) {
        dataset_service_free_uel_2d(ref_uel);
        char err[128];
        snprintf(err, sizeof(err), "ERROR: Failed to load target file %s", filename);
        reconstruction_viewer_view_set_status(presenter->view, err);
        return;
    }

    if (ref_n_meas != tgt_n_meas || ref_n_inj != tgt_n_inj) {
        dataset_service_free_uel_2d(ref_uel);
        dataset_service_free_uel_2d(tgt_uel);
        char err[128];
        snprintf(err, sizeof(err), "ERROR: Dimension mismatch ref[%dx%d] vs tgt[%dx%d]",
                 (int)ref_n_meas, (int)ref_n_inj, (int)tgt_n_meas, (int)tgt_n_inj);
        reconstruction_viewer_view_set_status(presenter->view, err);
        return;
    }

    reconstruction_viewer_view_set_status(presenter->view, "Performing reconstruction...");

    if (!lbp_get_matrix_info()) {
        reconstruction_viewer_view_set_status(presenter->view, "Initializing LBP (loading sensitivity matrix)...");

        if (!lbp_init()) {
            dataset_service_free_uel_2d(ref_uel);
            dataset_service_free_uel_2d(tgt_uel);
            reconstruction_viewer_view_set_status(presenter->view, "ERROR: Failed to load sensitivity_matrix.bin from SD card");
            return;
        }
    }

    presenter->recon_result = lbp_reconstruct(ref_uel[0], tgt_uel[0], ref_n_meas, ref_n_inj);

    dataset_service_free_uel_2d(ref_uel);
    dataset_service_free_uel_2d(tgt_uel);

    if (!presenter->recon_result || !presenter->recon_result->success) {
        char err[256];
        if (presenter->recon_result) {
            snprintf(err, sizeof(err), "RECON ERROR: %s", presenter->recon_result->error_msg);
        } else {
            snprintf(err, sizeof(err), "RECON ERROR: lbp_init not called or sensitivity_matrix.bin missing");
        }
        reconstruction_viewer_view_set_status(presenter->view, err);
        return;
    }

    reconstruction_viewer_view_render_rgb565(presenter->view,
                                            presenter->recon_result->color_buffer,
                                            presenter->recon_result->display_size,
                                            presenter->recon_result->display_size);

    char status[128];
    int32_t vmin_int = (int32_t)(presenter->recon_result->vmin * 1000000);
    int32_t vmax_int = (int32_t)(presenter->recon_result->vmax * 1000000);
    snprintf(status, sizeof(status), "Range: [%ld.%lduV, %ld.%lduV]",
             (long)(vmin_int / 1000000), (long)((vmin_int / 1000) % 1000),
             (long)(vmax_int / 1000000), (long)((vmax_int / 1000) % 1000));
    reconstruction_viewer_view_set_status(presenter->view, status);

    reconstruction_viewer_view_set_save_enabled(presenter->view, 1);
}

void reconstruction_viewer_presenter_on_return(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(recon_return_async_cb, p);
}

void reconstruction_viewer_presenter_on_save(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p || p->save_timer) return;

    lv_async_call(save_start_async_cb, p);
}

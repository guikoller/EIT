#include "reconstruction_viewer_presenter.h"

#include "app/app_coordinator.h"
#include "services/eit_acq_simulated.h"
#include "services/json_encoder.h"
#include "services/wifi/wifi_service.h"
#include "eit_config.h"

#include "stm32f7xx_hal.h"

#include <string.h>
#include <stdio.h>

#define DISPLAY_SIZE  EIT_DISPLAY_SIZE
#define DISPLAY_SIZE_MAX  EIT_DISPLAY_SIZE_MAX

/* Forward declaration for async streaming callback used inside acq_timer_cb */
static void stream_send_async_cb(void *user_data);

static void presenter_cleanup(reconstruction_viewer_presenter_t *p)
{
    if (!p) return;

    if (p->acq_timer) {
        lv_timer_del(p->acq_timer);
        p->acq_timer = NULL;
    }

    if (p->save_timer) {
        lv_timer_del(p->save_timer);
        p->save_timer = NULL;
    }

    /* recon_result points to static LBP buffer — just clear the pointer */
    p->recon_result = NULL;

    /* Stop streaming */
    p->streaming      = 0;
    p->upload_pending = 0;
    p->stream_is_first = 1;

    /* Destroy acquisition backend (frees all loaded data) */
    if (p->acq_backend) {
        eit_acquisition_deinit();
        eit_acq_simulated_destroy(p->acq_backend);
        p->acq_backend = NULL;
    }

    memset(&p->ref_frame, 0, sizeof(p->ref_frame));
    p->is_playing = 0;
    p->send_pending = 0;
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

static void recon_nav_home_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_cleanup(p);

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

static void recon_nav_eit_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_cleanup(p);

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

static void recon_nav_settings_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p) return;

    presenter_cleanup(p);

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}

/**
 * Build a unique file path from the current target filename + active algorithm.
 *
 * Example: filename = "datamat_4_3.bin", algo = EIT_ALGO_LBP, ext = "bmp"
 *          -> "0:/datamat_4_3_LBP.bmp"
 *
 * If the file already exists a numeric suffix is appended:
 *          -> "0:/datamat_4_3_LBP_001.bmp"
 */
static int make_unique_path(char *out, size_t outsz,
                            const char *filename, const char *ext)
{
    /* ---- Strip extension from filename ---- */
    char base[64];
    strncpy(base, filename, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    /* ---- Algorithm tag ---- */
    const char *algo_tag = "LBP";

    /* ---- First attempt: base_algo.ext ---- */
    FILINFO fno;
    snprintf(out, outsz, "0:/%s_%s.%s", base, algo_tag, ext);
    if (f_stat(out, &fno) != FR_OK) {
        return 1;   /* doesn't exist yet — use it */
    }

    /* ---- Fallback: base_algo_NNN.ext ---- */
    for (uint32_t i = 1; i < 1000; i++) {
        snprintf(out, outsz, "0:/%s_%s_%03lu.%s",
                 base, algo_tag, (unsigned long)i, ext);
        if (f_stat(out, &fno) != FR_OK) {
            return 1;
        }
    }
    return 0;
}

/* Backward-compatible wrapper */
static int make_unique_bmp_path(char *out, size_t outsz,
                                const char *filename)
{
    return make_unique_path(out, outsz, filename, "bmp");
}

/**
 * Convert a float to a decimal ASCII string using ONLY integer printf
 * formatters (%ld, %lu).  This avoids the need for float-aware printf
 * which is not available when linking with -nodefaultlibs.
 *
 * Output uses scientific notation: [-]D.DDDDDDeN
 * 7 significant digits — enough for float32 full precision.
 *
 * Returns the number of characters written (excluding NUL).
 */
static int float_to_str(char *buf, size_t bufsz, float val)
{
    /* NaN: val != val is the portable test */
    if (val != val) {
        if (bufsz >= 4) { buf[0]='N'; buf[1]='a'; buf[2]='N'; buf[3]='\0'; }
        return 3;
    }

    /* Zero */
    if (val == 0.0f) {
        if (bufsz >= 2) { buf[0]='0'; buf[1]='\0'; }
        return 1;
    }

    int neg = 0;
    if (val < 0.0f) { neg = 1; val = -val; }

    /* Find decimal exponent: normalise val into [1.0, 10.0) */
    int exp10 = 0;
    if (val >= 10.0f) {
        while (val >= 1000.0f) { val *= 0.001f; exp10 += 3; }
        while (val >= 10.0f)   { val *= 0.1f;   exp10 += 1; }
    } else if (val < 1.0f) {
        while (val < 0.001f) { val *= 1000.0f; exp10 -= 3; }
        while (val < 1.0f)   { val *= 10.0f;   exp10 -= 1; }
    }

    /* Extract 7 significant digits as integer */
    uint32_t sig = (uint32_t)(val * 1000000.0f + 0.5f);
    if (sig >= 10000000u) { sig /= 10u; exp10++; }

    /* Split into integer digit + 6 fractional digits */
    uint32_t d0   = sig / 1000000u;
    uint32_t frac = sig % 1000000u;

    /* Strip trailing zeros from fraction for cleaner output */
    int frac_digits = 6;
    while (frac_digits > 0 && (frac % 10u) == 0) {
        frac /= 10u;
        frac_digits--;
    }

    int len;
    if (frac_digits == 0 && exp10 == 0) {
        len = snprintf(buf, bufsz, "%s%lu",
                       neg ? "-" : "", (unsigned long)d0);
    } else if (frac_digits == 0) {
        len = snprintf(buf, bufsz, "%s%lue%d",
                       neg ? "-" : "", (unsigned long)d0, exp10);
    } else if (exp10 == 0) {
        len = snprintf(buf, bufsz, "%s%lu.%0*lu",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac);
    } else {
        len = snprintf(buf, bufsz, "%s%lu.%0*lue%d",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac, exp10);
    }
    return (len > 0) ? len : 0;
}

/**
 * Export the raw reconstruction matrix (float) as a CSV file.
 *
 * Writes EIT_IMAGE_SIZE rows × EIT_IMAGE_SIZE columns.
 * NaN values (pixels outside the circular mask) are written as "NaN".
 * Uses manual float→string to avoid needing float-aware printf.
 *
 * Large locals are static to avoid stack overflow in the LVGL async
 * callback context.
 *
 * Returns 1 on success, 0 on failure.
 */
static int save_csv_sync(const char *filename, const float *image_data,
                         uint32_t image_size)
{
    static char csv_path[64];
    if (!make_unique_path(csv_path, sizeof(csv_path), filename, "csv")) {
        return 0;
    }

    static FIL fil;
    FRESULT res = f_open(&fil, csv_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return 0;
    }

    /* Write row by row.  32 floats × ~15 chars each + commas ≈ ~512 chars */
    static char linebuf[768];

    for (uint32_t row = 0; row < image_size; row++) {
        uint32_t pos = 0;
        for (uint32_t col = 0; col < image_size; col++) {
            float val = image_data[row * image_size + col];

            int written = float_to_str(&linebuf[pos],
                                       sizeof(linebuf) - pos, val);
            if (written > 0) pos += (uint32_t)written;

            /* Comma separator (not after last column) */
            if (col < image_size - 1u && pos < sizeof(linebuf) - 1u) {
                linebuf[pos++] = ',';
            }
        }
        /* Newline */
        if (pos < sizeof(linebuf) - 1u) {
            linebuf[pos++] = '\n';
        }

        UINT bw = 0;
        res = f_write(&fil, linebuf, (UINT)pos, &bw);
        if (res != FR_OK || bw != (UINT)pos) {
            f_close(&fil);
            return 0;
        }
    }

    f_close(&fil);
    return 1;
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

    static uint8_t rowbuf[DISPLAY_SIZE_MAX * 3u];
    const uint32_t width = p->recon_result->display_size;
    const uint32_t height = p->recon_result->display_size;
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

    if (!make_unique_bmp_path(p->save_path, sizeof(p->save_path),
                             p->current_filename)) {
        reconstruction_viewer_view_set_status(p->view, "SAVE ERROR: name");
        return;
    }

    /* ---- Export raw data as CSV (synchronous, ~12 KB) ---- */
    if (p->recon_result->image_data) {
        save_csv_sync(p->current_filename,
                      p->recon_result->image_data,
                      p->recon_result->image_size);
    }

    /* ---- Start BMP save (asynchronous, timer-based) ---- */
    p->save_last_res = f_open(&p->save_file, p->save_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (p->save_last_res != FR_OK) {
        char err[64];
        snprintf(err, sizeof(err), "SAVE OPEN ERR r%u", (unsigned)p->save_last_res);
        reconstruction_viewer_view_set_status(p->view, err);
        return;
    }

    const uint32_t width = p->recon_result->display_size;
    const uint32_t height = p->recon_result->display_size;
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

/* ---------- Algorithm dispatch helper ---------- */
static ReconstructionResult *dispatch_reconstruct(
    const float *ref_uel, const float *target_uel,
    uint16_t n_meas, uint16_t n_inj)
{
    return lbp_reconstruct(ref_uel, target_uel, n_meas, n_inj);
}

/* ---------- Live-stream send callback (runs in lv_async) ---------- */
static void stream_send_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p) return;

    if (p->stream_json_len > 0u) {
        const char *json_buf = (const char *)EIT_SDRAM_JSON_BUF_ADDR;
        char reply[64];

        if (wifi_service_init() == 0) {
            wifi_service_post_buffer("/api/frame",
                                     json_buf, p->stream_json_len,
                                     reply, sizeof(reply));
        }
    }

    p->upload_pending = 0;
}

/* ---------- Continuous acquisition timer ---------- */
static void acq_timer_cb(lv_timer_t *t)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)lv_timer_get_user_data(t);
    if (!p || !p->view || !p->acq_backend) return;

    eit_acq_status_t status = eit_acquisition_poll();

    switch (status.state) {
    case EIT_ACQ_INJECTING:
    case EIT_ACQ_MEASURING:
        return;  /* Wait for frame to complete */

    case EIT_ACQ_FRAME_READY: {
        eit_frame_t frame;
        if (!eit_acquisition_get_frame(&frame)) return;

        p->recon_result = dispatch_reconstruct(p->ref_frame.uel, frame.uel,
                                               p->acq_n_meas, p->acq_n_inj);

        if (p->recon_result && p->recon_result->success) {
            reconstruction_viewer_view_render_rgb565(
                p->view,
                p->recon_result->color_buffer,
                p->recon_result->display_size,
                p->recon_result->display_size);
        }

        /* FPS tracking — update every second */
        p->frame_count++;
        uint32_t now = lv_tick_get();
        uint32_t elapsed = now - p->fps_tick_start;
        if (elapsed >= 1000u) {
            uint32_t fps_x10 = (p->frame_count * 10000u) / elapsed;
            reconstruction_viewer_view_set_fps(p->view, fps_x10);
            p->frame_count = 0;
            p->fps_tick_start = now;
        }

        /* Live streaming — encode compact frame JSON into SDRAM scratch,
         * then fire an async TCP send.  We drop this frame if a previous
         * send is still in flight (upload_pending) to prevent queuing. */
        if (p->streaming && !p->upload_pending) {
            char *json_buf = (char *)EIT_SDRAM_JSON_BUF_ADDR;
            json_encoder_t enc;
            json_init(&enc, json_buf, EIT_SDRAM_JSON_BUF_SIZE);

            json_object_start(&enc);
            json_key_uint(&enc, "f", frame.frame_number);
            json_key_uint(&enc, "t", frame.timestamp_ms);
            json_key_int(&enc, "ref", p->stream_is_first ? 1 : 0);
            json_key_uint(&enc, "nm", (uint32_t)frame.n_meas);
            json_key_uint(&enc, "ni", (uint32_t)frame.n_inj);

            uint32_t uel_count = (uint32_t)frame.n_meas * frame.n_inj;
            json_key_array(&enc, "uel");
            for (uint32_t i = 0; i < uel_count; i++) {
                json_float(&enc, frame.uel[i]);
            }
            json_array_end(&enc);

            if (p->recon_result && p->recon_result->success && p->recon_result->image_data) {
                uint32_t n_px = (uint32_t)p->recon_result->image_size *
                                (uint32_t)p->recon_result->image_size;
                json_key_array(&enc, "px");
                for (uint32_t i = 0; i < n_px; i++) {
                    json_float(&enc, p->recon_result->image_data[i]);
                }
                json_array_end(&enc);
            }
            json_object_end(&enc);

            if (!json_has_error(&enc)) {
                p->stream_json_len = (uint32_t)json_get_length(&enc);
                p->stream_is_first = 0;
                p->upload_pending  = 1;
                lv_async_call(stream_send_async_cb, p);
            }
        }

        /* Start next acquisition cycle */
        eit_acquisition_start_frame();
        return;
    }

    default:
        return;
    }
}

void reconstruction_viewer_presenter_init(reconstruction_viewer_presenter_t *presenter, reconstruction_viewer_view_t *view)
{
    if (!presenter) return;

    presenter_cleanup(presenter);
    memset(presenter, 0, sizeof(*presenter));

    presenter->view = view;
    presenter->save_last_res = FR_OK;
    presenter->noise_enabled = 0;
    presenter->noise_level_pct = EIT_NOISE_LEVEL_DEFAULT;
}

void reconstruction_viewer_presenter_on_create(reconstruction_viewer_presenter_t *presenter, const char *filename)
{
    if (!presenter || !presenter->view || !filename) return;

    strncpy(presenter->current_filename, filename, sizeof(presenter->current_filename) - 1);
    presenter->current_filename[sizeof(presenter->current_filename) - 1] = '\0';

    reconstruction_viewer_view_set_title(presenter->view, filename);
    reconstruction_viewer_view_set_save_enabled(presenter->view, 0);
    reconstruction_viewer_view_set_status(presenter->view, "Initializing acquisition...");

    /* Show which algorithm is active */
    {
        const app_state_t *st = app_coordinator_get_state();
        reconstruction_viewer_view_set_algorithm(presenter->view, "LBP");

        /* Apply display size from settings */
        uint32_t dsz = eit_display_size_for_setting(st->settings.image_size);
        reconstruction_viewer_view_set_display_size(presenter->view, dsz);
    }

    /* ---- Create simulated acquisition backend ---- */
    presenter->acq_backend = eit_acq_simulated_create("datamat_1_0.bin", filename);
    if (!presenter->acq_backend) {
        reconstruction_viewer_view_set_status(presenter->view,
            "ERROR: Failed to load reference or target data from SD");
        return;
    }

    eit_acquisition_init(presenter->acq_backend);

    /* Apply current noise settings to backend */
    eit_acq_simulated_set_noise(presenter->acq_backend,
                                 presenter->noise_enabled,
                                 presenter->noise_level_pct);

    /* Get reference frame (immediate, no acquisition delay) */
    if (!eit_acquisition_get_ref_frame(&presenter->ref_frame)) {
        reconstruction_viewer_view_set_status(presenter->view,
            "ERROR: Failed to get reference frame");
        eit_acquisition_deinit();
        eit_acq_simulated_destroy(presenter->acq_backend);
        presenter->acq_backend = NULL;
        return;
    }

    presenter->acq_n_meas = presenter->ref_frame.n_meas;
    presenter->acq_n_inj  = presenter->ref_frame.n_inj;

    /* Initialize LBP if needed */
    if (!lbp_get_matrix_info()) {
        reconstruction_viewer_view_set_status(presenter->view, "Loading sensitivity matrix...");

        if (!lbp_init()) {
            eit_acquisition_deinit();
            eit_acq_simulated_destroy(presenter->acq_backend);
            presenter->acq_backend = NULL;
            reconstruction_viewer_view_set_status(presenter->view,
                "ERROR: Failed to load sensitivity_matrix.bin from SD card");
            return;
        }
    }

    /* Get initial frame for immediate display (first get_frame works without start_frame) */
    reconstruction_viewer_view_set_status(presenter->view, "First reconstruction...");

    eit_frame_t first_frame;
    if (eit_acquisition_get_frame(&first_frame)) {
        presenter->recon_result = dispatch_reconstruct(presenter->ref_frame.uel, first_frame.uel,
                                                       presenter->acq_n_meas, presenter->acq_n_inj);

        if (presenter->recon_result && presenter->recon_result->success) {
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
        }
    }

    reconstruction_viewer_view_set_save_enabled(presenter->view, 1);
    reconstruction_viewer_view_set_send_enabled(presenter->view, 1);

    /* Create acquisition timer (initially paused — press PLAY to start loop) */
    presenter->acq_timer = lv_timer_create(acq_timer_cb, 0, presenter);
    lv_timer_pause(presenter->acq_timer);
    presenter->is_playing = 0;
    presenter->frame_count = 0;
    presenter->fps_tick_start = lv_tick_get();
    reconstruction_viewer_view_set_play_state(presenter->view, 0);
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

    /* Auto-pause acquisition during save */
    if (p->is_playing && p->acq_timer) {
        p->is_playing = 0;
        lv_timer_pause(p->acq_timer);
        reconstruction_viewer_view_set_play_state(p->view, 0);
    }

    lv_async_call(save_start_async_cb, p);
}

void reconstruction_viewer_presenter_on_play_pause(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p || !p->acq_timer || !p->acq_backend) return;

    p->is_playing = !p->is_playing;

    if (p->is_playing) {
        p->frame_count = 0;
        p->fps_tick_start = lv_tick_get();
        eit_acquisition_start_frame();
        lv_timer_resume(p->acq_timer);
    } else {
        lv_timer_pause(p->acq_timer);
    }

    reconstruction_viewer_view_set_play_state(p->view, p->is_playing);
}

void reconstruction_viewer_presenter_on_noise_toggle(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    p->noise_enabled = !p->noise_enabled;
    if (p->acq_backend) {
        eit_acq_simulated_set_noise(p->acq_backend, p->noise_enabled, p->noise_level_pct);
    }
    reconstruction_viewer_view_set_noise_state(p->view, p->noise_enabled);
}

void reconstruction_viewer_presenter_on_noise_level(void *ctx, int32_t level)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    if (level < 0) level = 0;
    if (level > EIT_NOISE_LEVEL_MAX) level = EIT_NOISE_LEVEL_MAX;
    p->noise_level_pct = level;
    if (p->acq_backend) {
        eit_acq_simulated_set_noise(p->acq_backend, p->noise_enabled, p->noise_level_pct);
    }
    reconstruction_viewer_view_set_noise_level(p->view, level);
}

/**
 * Build EIT measurement data as JSON and save to SD card.
 */
static int build_and_save_json(reconstruction_viewer_presenter_t *p)
{
    if (!p || !p->view) return 0;

    /* Force a frame acquisition if needed (when paused) */
    eit_acquisition_start_frame();

    /* Poll until frame is ready (with timeout) */
    uint32_t start = HAL_GetTick();
    eit_acq_status_t status;
    do {
        status = eit_acquisition_poll();
        if (status.state == EIT_ACQ_FRAME_READY) break;
        if (HAL_GetTick() - start > 1000) break;  /* 1s timeout */
    } while (status.state == EIT_ACQ_INJECTING || status.state == EIT_ACQ_MEASURING);

    /* Get current frame data */
    eit_frame_t current_frame;
    if (!eit_acquisition_get_frame(&current_frame)) {
        reconstruction_viewer_view_set_status(p->view, "RECORD ERROR: no frame available");
        return 0;
    }

    /* Generate unique JSON filename */
    if (!make_unique_path(p->recorded_json_path, sizeof(p->recorded_json_path),
                          p->current_filename, "json")) {
        reconstruction_viewer_view_set_status(p->view, "RECORD ERROR: filename");
        return 0;
    }

    /* Use SDRAM buffer for JSON */
    char *json_buf = (char *)EIT_SDRAM_JSON_BUF_ADDR;
    json_encoder_t enc;
    json_init(&enc, json_buf, EIT_SDRAM_JSON_BUF_SIZE);

    /* Build JSON structure */
    json_object_start(&enc);

    /* Device info */
    json_key_string(&enc, "device_id", "EIT-F769-001");
    json_key_uint(&enc, "timestamp", HAL_GetTick() / 1000);
    json_key_uint(&enc, "frame_number", current_frame.frame_number);

    /* Configuration */
    json_key_object(&enc, "config");
    json_key_uint(&enc, "n_electrodes", EIT_NUM_ELECTRODES);
    json_key_uint(&enc, "n_meas", current_frame.n_meas);
    json_key_uint(&enc, "n_inj", current_frame.n_inj);
    json_key_uint(&enc, "image_size", EIT_IMAGE_SIZE);
    json_object_end(&enc);

    /* Source file */
    json_key_string(&enc, "source_file", p->current_filename);

    /* Measurements - voltage data array */
    json_key_object(&enc, "measurements");
    uint32_t uel_count = (uint32_t)current_frame.n_meas * current_frame.n_inj;
    json_key_uint(&enc, "uel_count", uel_count);
    json_key_array(&enc, "uel");
    for (uint32_t i = 0; i < uel_count; i++) {
        json_float(&enc, current_frame.uel[i]);
    }
    json_array_end(&enc);
    json_object_end(&enc);

    /* Reconstruction metadata (if available) */
    if (p->recon_result && p->recon_result->success) {
        json_key_object(&enc, "reconstruction");
        json_key_string(&enc, "algorithm", "LBP");
        json_key_float(&enc, "vmin", p->recon_result->vmin);
        json_key_float(&enc, "vmax", p->recon_result->vmax);
        json_object_end(&enc);
    }

    json_object_end(&enc);

    /* Check for JSON encoding errors */
    if (json_has_error(&enc)) {
        reconstruction_viewer_view_set_status(p->view, "RECORD ERROR: JSON overflow");
        return 0;
    }

    /* Save to SD card */
    FIL fil;
    FRESULT res = f_open(&fil, p->recorded_json_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        char err[64];
        snprintf(err, sizeof(err), "RECORD ERROR: open r%u", (unsigned)res);
        reconstruction_viewer_view_set_status(p->view, err);
        return 0;
    }

    size_t json_len = json_get_length(&enc);
    UINT bw = 0;
    res = f_write(&fil, json_buf, (UINT)json_len, &bw);
    f_close(&fil);

    if (res != FR_OK || bw != (UINT)json_len) {
        char err[64];
        snprintf(err, sizeof(err), "RECORD ERROR: write r%u", (unsigned)res);
        reconstruction_viewer_view_set_status(p->view, err);
        return 0;
    }

    return 1;
}

static void send_async_cb(void *user_data)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)user_data;
    if (!p || !p->view) return;

    if (!p->has_recorded_data) {
        if (!build_and_save_json(p)) {
            p->send_pending = 0;
            reconstruction_viewer_view_set_send_enabled(p->view, 1);
            return;
        }
        p->has_recorded_data = 1;
    }

    if (wifi_service_init() != 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "SEND ERROR: %s", wifi_service_last_error());
        reconstruction_viewer_view_set_status(p->view, msg);
        p->send_pending = 0;
        reconstruction_viewer_view_set_send_enabled(p->view, 1);
        return;
    }

    if (wifi_service_send_json_file(p->recorded_json_path,
                                    p->send_reply,
                                    sizeof(p->send_reply)) != 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "SEND ERROR: %s", wifi_service_last_error());
        reconstruction_viewer_view_set_status(p->view, msg);
        p->send_pending = 0;
        reconstruction_viewer_view_set_send_enabled(p->view, 1);
        return;
    }

    {
        char msg[96];
        const char *fn = p->recorded_json_path;
        if (strncmp(fn, "0:/", 3) == 0) fn += 3;
        snprintf(msg, sizeof(msg), "Sent: %s", fn);
        reconstruction_viewer_view_set_status(p->view, msg);
    }

    p->send_pending = 0;
    reconstruction_viewer_view_set_send_enabled(p->view, 1);
}

void reconstruction_viewer_presenter_on_record(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p || !p->view) return;

    /* Auto-pause acquisition during record */
    if (p->is_playing && p->acq_timer) {
        p->is_playing = 0;
        lv_timer_pause(p->acq_timer);
        reconstruction_viewer_view_set_play_state(p->view, 0);
    }

    /* Disable button during operation */
    reconstruction_viewer_view_set_record_enabled(p->view, 0);
    reconstruction_viewer_view_set_status(p->view, "Recording...");

    /* Build and save JSON */
    if (build_and_save_json(p)) {
        p->has_recorded_data = 1;

        /* Show success message with filename (skip "0:/" prefix) */
        char msg[96];
        const char *fn = p->recorded_json_path;
        if (strncmp(fn, "0:/", 3) == 0) fn += 3;
        snprintf(msg, sizeof(msg), "Recorded: %s", fn);
        reconstruction_viewer_view_set_status(p->view, msg);
    }

    /* Re-enable button */
    reconstruction_viewer_view_set_record_enabled(p->view, 1);
}

void reconstruction_viewer_presenter_on_send(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p || !p->view || p->send_pending) return;

    if (p->is_playing && p->acq_timer) {
        p->is_playing = 0;
        lv_timer_pause(p->acq_timer);
        reconstruction_viewer_view_set_play_state(p->view, 0);
    }

    p->send_pending = 1;
    reconstruction_viewer_view_set_send_enabled(p->view, 0);
    reconstruction_viewer_view_set_status(p->view, "Sending JSON...");

    lv_async_call(send_async_cb, p);
}

void reconstruction_viewer_presenter_on_nav_home(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(recon_nav_home_async_cb, p);
}

void reconstruction_viewer_presenter_on_nav_eit(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(recon_nav_eit_async_cb, p);
}

void reconstruction_viewer_presenter_on_nav_settings(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p) return;

    lv_async_call(recon_nav_settings_async_cb, p);
}

void reconstruction_viewer_presenter_on_stream(void *ctx)
{
    reconstruction_viewer_presenter_t *p = (reconstruction_viewer_presenter_t *)ctx;
    if (!p || !p->view) return;

    p->streaming = !p->streaming;
    if (p->streaming) {
        p->stream_is_first = 1;
        p->upload_pending  = 0;
        reconstruction_viewer_view_set_stream_state(p->view, 1);
        reconstruction_viewer_view_set_status(p->view, "Streaming...");
    } else {
        reconstruction_viewer_view_set_stream_state(p->view, 0);
        reconstruction_viewer_view_set_status(p->view, "Stream stopped");
    }
}

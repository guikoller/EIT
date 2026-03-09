/**
 * Batch Processing Service — implementation
 *
 * Scans SD for datamat_*.bin files, uses datamat_1_0.bin as reference,
 * and for every other dataset runs LBP, saves BMP + CSV into "0:/batch/".
 *
 * Designed to be called one-step-at-a-time from an LVGL timer so the
 * UI stays responsive.
 */
#include "batch_process.h"

#include "storage_service.h"
#include "dataset_service.h"
#include "algorithms/lbp_reconstruction.h"
#include "app/app_coordinator.h"
#include "eit_config.h"
#include "ff.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */
#define BATCH_DIR       "0:/batch"
#define REF_FILENAME    "datamat_1_0.bin"
#define BATCH_MAX_FILES EIT_MAX_BROWSER_FILES

/** List of target filenames (excludes the reference). */
static char      s_targets[BATCH_MAX_FILES][STORAGE_FILENAME_MAX];
static uint16_t  s_n_targets;
static uint16_t  s_cursor;          /**< Next index to process */

/** Reference data (loaded once, freed at the end). */
static float   **s_ref_uel  = NULL;
static uint16_t  s_ref_n_meas;
static uint16_t  s_ref_n_inj;

static batch_status_t s_status = BATCH_IDLE;
static char s_error[64];

/* Case-insensitive strcmp (ASCII only) */
static int strcasecmp_a(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ------------------------------------------------------------------ */
/*  float → ASCII  (duplicated from reconstruction_viewer_presenter)   */
/* ------------------------------------------------------------------ */
static int float_to_str(char *buf, size_t bufsz, float val)
{
    if (val != val) {
        if (bufsz >= 4) { buf[0]='N'; buf[1]='a'; buf[2]='N'; buf[3]='\0'; }
        return 3;
    }
    if (val == 0.0f) {
        if (bufsz >= 2) { buf[0]='0'; buf[1]='\0'; }
        return 1;
    }

    int neg = 0;
    if (val < 0.0f) { neg = 1; val = -val; }

    int exp10 = 0;
    if (val >= 10.0f) {
        while (val >= 1000.0f) { val *= 0.001f; exp10 += 3; }
        while (val >= 10.0f)   { val *= 0.1f;   exp10 += 1; }
    } else if (val < 1.0f) {
        while (val < 0.001f) { val *= 1000.0f; exp10 -= 3; }
        while (val < 1.0f)   { val *= 10.0f;   exp10 -= 1; }
    }

    uint32_t sig = (uint32_t)(val * 1000000.0f + 0.5f);
    if (sig >= 10000000u) { sig /= 10u; exp10++; }

    uint32_t d0   = sig / 1000000u;
    uint32_t frac = sig % 1000000u;

    int frac_digits = 6;
    while (frac_digits > 0 && (frac % 10u) == 0) {
        frac /= 10u;
        frac_digits--;
    }

    int len;
    if (frac_digits == 0 && exp10 == 0)
        len = snprintf(buf, bufsz, "%s%lu",
                       neg ? "-" : "", (unsigned long)d0);
    else if (frac_digits == 0)
        len = snprintf(buf, bufsz, "%s%lue%d",
                       neg ? "-" : "", (unsigned long)d0, exp10);
    else if (exp10 == 0)
        len = snprintf(buf, bufsz, "%s%lu.%0*lu",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac);
    else
        len = snprintf(buf, bufsz, "%s%lu.%0*lue%d",
                       neg ? "-" : "", (unsigned long)d0,
                       frac_digits, (unsigned long)frac, exp10);
    return (len > 0) ? len : 0;
}

/* ------------------------------------------------------------------ */
/*  CSV writer (synchronous)                                           */
/* ------------------------------------------------------------------ */
static int save_csv(const char *path, const float *data, uint32_t sz)
{
    static FIL fil;
    FRESULT res = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) return 0;

    static char line[768];
    for (uint32_t r = 0; r < sz; r++) {
        uint32_t pos = 0;
        for (uint32_t c = 0; c < sz; c++) {
            int w = float_to_str(&line[pos], sizeof(line) - pos,
                                 data[r * sz + c]);
            if (w > 0) pos += (uint32_t)w;
            if (c < sz - 1u && pos < sizeof(line) - 1u)
                line[pos++] = ',';
        }
        if (pos < sizeof(line) - 1u) line[pos++] = '\n';

        UINT bw = 0;
        res = f_write(&fil, line, (UINT)pos, &bw);
        if (res != FR_OK || bw != (UINT)pos) {
            f_close(&fil);
            return 0;
        }
    }
    f_close(&fil);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  BMP writer (synchronous — no timer needed for batch)               */
/* ------------------------------------------------------------------ */
static void wr_u16(uint8_t *d, uint16_t v) { d[0]=(uint8_t)(v&0xFF); d[1]=(uint8_t)(v>>8); }
static void wr_u32(uint8_t *d, uint32_t v) { d[0]=(uint8_t)(v); d[1]=(uint8_t)(v>>8); d[2]=(uint8_t)(v>>16); d[3]=(uint8_t)(v>>24); }
static void wr_i32(uint8_t *d, int32_t  v) { wr_u32(d,(uint32_t)v); }

static int save_bmp(const char *path, const uint16_t *rgb565,
                    uint32_t width, uint32_t height)
{
    static FIL fil;
    FRESULT res = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) return 0;

    uint32_t row_bytes   = width * 3u;
    uint32_t pixel_bytes = row_bytes * height;
    uint32_t file_size   = 54u + pixel_bytes;

    /* BMP header (54 bytes) */
    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    wr_u32(&hdr[2],  file_size);
    wr_u32(&hdr[10], 54u);
    wr_u32(&hdr[14], 40u);
    wr_i32(&hdr[18], (int32_t)width);
    wr_i32(&hdr[22], -(int32_t)height);   /* top-down */
    wr_u16(&hdr[26], 1u);
    wr_u16(&hdr[28], 24u);
    wr_u32(&hdr[34], pixel_bytes);

    UINT bw = 0;
    res = f_write(&fil, hdr, sizeof(hdr), &bw);
    if (res != FR_OK || bw != sizeof(hdr)) { f_close(&fil); return 0; }

    /* Pixel rows — convert RGB565 → BGR24 */
    static uint8_t rowbuf[EIT_DISPLAY_SIZE_MAX * 3u];
    for (uint32_t y = 0; y < height; y++) {
        const uint16_t *src = rgb565 + y * width;
        uint8_t *dst = rowbuf;
        for (uint32_t x = 0; x < width; x++) {
            uint16_t px = src[x];
            uint8_t r5 = (uint8_t)((px >> 11) & 0x1Fu);
            uint8_t g6 = (uint8_t)((px >>  5) & 0x3Fu);
            uint8_t b5 = (uint8_t)( px        & 0x1Fu);
            *dst++ = (uint8_t)((b5 << 3) | (b5 >> 2));
            *dst++ = (uint8_t)((g6 << 2) | (g6 >> 4));
            *dst++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        }
        bw = 0;
        res = f_write(&fil, rowbuf, (UINT)row_bytes, &bw);
        if (res != FR_OK || bw != (UINT)row_bytes) { f_close(&fil); return 0; }
    }

    f_close(&fil);
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Build output path:  "0:/batch/<base>_LBP.ext"                      */
/* ------------------------------------------------------------------ */
static void make_batch_path(char *out, size_t outsz,
                            const char *filename, const char *ext)
{
    char base[64];
    strncpy(base, filename, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    snprintf(out, outsz, BATCH_DIR "/%s_LBP.%s", base, ext);
}

/* ------------------------------------------------------------------ */
/*  Cleanup helper                                                     */
/* ------------------------------------------------------------------ */
static void cleanup_ref(void)
{
    if (s_ref_uel) {
        dataset_service_free_uel_2d(s_ref_uel);
        s_ref_uel = NULL;
    }
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

const char *batch_process_error(void)
{
    return s_error;
}

int batch_process_begin(void)
{
    s_status    = BATCH_IDLE;
    s_n_targets = 0;
    s_cursor    = 0;
    s_error[0]  = '\0';

    /* ---- SD mounted? ---- */
    if (!storage_service_is_mounted()) {
        snprintf(s_error, sizeof(s_error), "NO SD");
        return 0;
    }

    /* ---- LBP ready? ---- */
    if (!lbp_init()) {
        snprintf(s_error, sizeof(s_error), "LBP INIT (no sens?)");
        return 0;
    }

    /* ---- Create output directory ---- */
    {
        FRESULT res = f_mkdir(BATCH_DIR);
        if (res != FR_OK && res != FR_EXIST) {
            snprintf(s_error, sizeof(s_error), "MKDIR r%u", (unsigned)res);
            return 0;
        }
    }

    /* ---- Scan root for .bin files ---- */
    static storage_file_entry_t entries[BATCH_MAX_FILES];
    int count = 0;
    if (storage_service_scan_root(entries, BATCH_MAX_FILES, &count) < 0) {
        snprintf(s_error, sizeof(s_error), "SCAN FAIL");
        return 0;
    }

    /* ---- Separate reference from targets ---- */
    int found_ref = 0;
    char ref_actual[STORAGE_FILENAME_MAX] = {0};
    for (int i = 0; i < count; i++) {
        /* Case-insensitive compare with reference filename */
        if (strcasecmp_a(entries[i].filename, REF_FILENAME) == 0) {
            found_ref = 1;
            strncpy(ref_actual, entries[i].filename, STORAGE_FILENAME_MAX - 1);
            continue;   /* skip ref — it's loaded separately */
        }
        if (s_n_targets < BATCH_MAX_FILES) {
            strncpy(s_targets[s_n_targets], entries[i].filename,
                    STORAGE_FILENAME_MAX - 1);
            s_targets[s_n_targets][STORAGE_FILENAME_MAX - 1] = '\0';
            s_n_targets++;
        }
    }
    if (!found_ref) {
        snprintf(s_error, sizeof(s_error), "NO REF (%d bins)", count);
        return 0;
    }
    if (s_n_targets == 0) {
        snprintf(s_error, sizeof(s_error), "NO TARGETS (%d bins)", count);
        return 0;
    }

    /* ---- Load reference dataset once (use actual filename from dir) ---- */
    s_ref_uel = dataset_service_load_uel_2d(ref_actual,
                                            &s_ref_n_meas, &s_ref_n_inj);
    if (!s_ref_uel) {
        snprintf(s_error, sizeof(s_error), "REF LOAD FAIL");
        return 0;
    }

    s_status = BATCH_RUNNING;
    return 1;
}

batch_status_t batch_process_step(batch_progress_t *progress)
{
    if (progress) {
        memset(progress, 0, sizeof(*progress));
        progress->total = s_n_targets;
    }

    if (s_status != BATCH_RUNNING) {
        if (progress) progress->status = s_status;
        return s_status;
    }

    /* All done? */
    if (s_cursor >= s_n_targets) {
        cleanup_ref();
        s_status = BATCH_DONE;
        if (progress) progress->status = BATCH_DONE;
        return BATCH_DONE;
    }

    const char *tgt_name = s_targets[s_cursor];
    if (progress) {
        strncpy(progress->current_file, tgt_name, sizeof(progress->current_file) - 1);
        progress->current = s_cursor + 1u;
    }

    /* ---- Load target dataset ---- */
    uint16_t tgt_n_meas = 0, tgt_n_inj = 0;
    float **tgt_uel = dataset_service_load_uel_2d(tgt_name,
                                                  &tgt_n_meas, &tgt_n_inj);
    if (!tgt_uel) {
        /* Skip this file — continue with next */
        s_cursor++;
        if (progress) {
            progress->status = BATCH_RUNNING;
            snprintf(progress->error_msg, sizeof(progress->error_msg),
                     "SKIP %s (load fail)", tgt_name);
        }
        return BATCH_RUNNING;
    }

    /* Dimension mismatch? Skip. */
    if (tgt_n_meas != s_ref_n_meas || tgt_n_inj != s_ref_n_inj) {
        dataset_service_free_uel_2d(tgt_uel);
        s_cursor++;
        if (progress) {
            progress->status = BATCH_RUNNING;
            snprintf(progress->error_msg, sizeof(progress->error_msg),
                     "SKIP %s (dim)", tgt_name);
        }
        return BATCH_RUNNING;
    }

    /* ---- Run LBP reconstruction ---- */
    ReconstructionResult *res = lbp_reconstruct(
        s_ref_uel[0], tgt_uel[0], tgt_n_meas, tgt_n_inj);

    int ok = 0;
    if (res && res->success) {
        /* ---- Save CSV ---- */
        static char csv_path[96];
        make_batch_path(csv_path, sizeof(csv_path), tgt_name, "csv");
        save_csv(csv_path, res->image_data, res->image_size);

        /* ---- Save BMP ---- */
        static char bmp_path[96];
        make_batch_path(bmp_path, sizeof(bmp_path), tgt_name, "bmp");
        if (res->color_buffer) {
            save_bmp(bmp_path, res->color_buffer,
                     res->display_size, res->display_size);
        }
        ok = 1;
    }

    dataset_service_free_uel_2d(tgt_uel);
    s_cursor++;

    if (progress) {
        progress->status = BATCH_RUNNING;
        if (!ok) {
            snprintf(progress->error_msg, sizeof(progress->error_msg),
                     "SKIP %s (recon fail)", tgt_name);
        }
    }

    /* Check if that was the last one */
    if (s_cursor >= s_n_targets) {
        cleanup_ref();
        s_status = BATCH_DONE;
        if (progress) progress->status = BATCH_DONE;
        return BATCH_DONE;
    }

    return BATCH_RUNNING;
}

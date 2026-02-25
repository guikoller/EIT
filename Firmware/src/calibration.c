#include "calibration.h"

#include "dataset_format.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENS_MAGIC 0x53454E53u

/* Image size is fixed to 32x32 for the firmware pipeline. */
#define CAL_IMAGE_SIZE 32u
#define CAL_MAX_PIXELS (CAL_IMAGE_SIZE * CAL_IMAGE_SIZE)

/* Use SDRAM for the big precomputed electrode field buffer to avoid heap use.
 * Must not overlap LTDC framebuffers, LBP matrix (0xC0400000) or color buffer (0xC0900000).
 */
#define CAL_ELEC_FIELD_ADDR ((uint32_t)0xC0A00000u)

typedef struct {
    uint32_t magic;
    uint32_t n_measurements;
    uint32_t n_pixels;
    uint32_t image_size;
    uint32_t n_inj;
    uint32_t n_meas;
    uint32_t reserved[2];
} SensitivityMatrixHeader;

_Static_assert(sizeof(SensitivityMatrixHeader) == 32, "SensitivityMatrixHeader must be 32 bytes");

static FIL s_out;
static calib_status_t s_status = CALIB_STATUS_IDLE;
static calib_stage_t s_stage = CALIB_STAGE_NONE;
static FRESULT s_last_res = FR_OK;

static uint16_t s_image_size;
static uint16_t s_n_inj;
static uint16_t s_n_meas;
static uint16_t s_n_elec;

static uint32_t s_n_pixels;
static uint32_t s_n_rows_total;

static uint32_t s_bytes_written;
static uint32_t s_bytes_total;

static uint32_t s_inj_idx;
static uint32_t s_meas_idx;

static int8_t *s_cp = NULL; /* [n_elec * n_inj] */
static int8_t *s_mp = NULL; /* [n_elec * n_meas] */

/* elec_field[e][2][p] stored as contiguous floats: e*(2*n_pixels) + comp*n_pixels + p */
static float *s_elec_field = NULL;

/* Per-row working buffers kept out of heap (similar spirit to BMP saver). */
static float s_inj_field_buf[2u * CAL_MAX_PIXELS];
static float s_row_buf_buf[CAL_MAX_PIXELS];

static void free_work_buffers(void)
{
    if (s_cp) { free(s_cp); s_cp = NULL; }
    if (s_mp) { free(s_mp); s_mp = NULL; }
    s_elec_field = NULL;
}

static void reset_state(void)
{
    memset(&s_out, 0, sizeof(s_out));
    s_status = CALIB_STATUS_IDLE;
    s_stage = CALIB_STAGE_NONE;
    s_last_res = FR_OK;

    s_image_size = 0;
    s_n_inj = 0;
    s_n_meas = 0;
    s_n_elec = 0;

    s_n_pixels = 0;
    s_n_rows_total = 0;

    s_bytes_written = 0;
    s_bytes_total = 0;

    s_inj_idx = 0;
    s_meas_idx = 0;
}

static calib_status_t fail(calib_stage_t stage, FRESULT res)
{
    s_stage = stage;
    s_last_res = res;
    if (s_status == CALIB_STATUS_RUNNING) {
        f_close(&s_out);
    }
    free_work_buffers();
    s_status = CALIB_STATUS_ERROR;
    return s_status;
}

static void precompute_electrode_fields(float grid_extent, float min_dist)
{
    const float eps = 1e-12f;
    const float two_pi = 6.2831853071795864769f;

    float x0 = -grid_extent;
    float y0 = -grid_extent;
    float dx = (2.0f * grid_extent) / (float)(s_image_size - 1);
    float dy = (2.0f * grid_extent) / (float)(s_image_size - 1);

    for (uint16_t e = 0; e < s_n_elec; e++) {
        float angle = ((float)e * two_pi) / (float)s_n_elec;
        float ex = cosf(angle);
        float ey = sinf(angle);

        for (uint16_t iy = 0; iy < s_image_size; iy++) {
            float y = y0 + (float)iy * dy;
            for (uint16_t ix = 0; ix < s_image_size; ix++) {
                float x = x0 + (float)ix * dx;

                float rx = x - ex;
                float ry = y - ey;
                float dist = sqrtf(rx * rx + ry * ry);
                float dc = dist < min_dist ? min_dist : dist;

                float denom = (dc * dc) + eps;
                float fx = rx / denom;
                float fy = ry / denom;

                uint32_t p = (uint32_t)iy * (uint32_t)s_image_size + (uint32_t)ix;
                s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 0u * s_n_pixels + p] = fx;
                s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 1u * s_n_pixels + p] = fy;
            }
        }
    }
}

static void build_inj_field(uint16_t inj)
{
    float *ix = &s_inj_field_buf[0u * s_n_pixels];
    float *iy = &s_inj_field_buf[1u * s_n_pixels];

    memset(ix, 0, s_n_pixels * sizeof(float));
    memset(iy, 0, s_n_pixels * sizeof(float));

    for (uint16_t e = 0; e < s_n_elec; e++) {
        int8_t w8 = s_cp[(uint32_t)e * (uint32_t)s_n_inj + (uint32_t)inj];
        if (w8 == 0) continue;
        float w = (float)w8;

        const float *fx = &s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 0u * s_n_pixels];
        const float *fy = &s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 1u * s_n_pixels];

        for (uint32_t p = 0; p < s_n_pixels; p++) {
            ix[p] += w * fx[p];
            iy[p] += w * fy[p];
        }
    }
}

static void compute_row(uint16_t inj, uint16_t meas)
{
    const float *ix = &s_inj_field_buf[0u * s_n_pixels];
    const float *iy = &s_inj_field_buf[1u * s_n_pixels];

    float maxabs = 0.0f;
    for (uint32_t p = 0; p < s_n_pixels; p++) {
        float mx = 0.0f;
        float my = 0.0f;

        for (uint16_t e = 0; e < s_n_elec; e++) {
            int8_t w8 = s_mp[(uint32_t)e * (uint32_t)s_n_meas + (uint32_t)meas];
            if (w8 == 0) continue;
            float w = (float)w8;

            float fx = s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 0u * s_n_pixels + p];
            float fy = s_elec_field[(uint32_t)e * (2u * s_n_pixels) + 1u * s_n_pixels + p];
            mx += w * fx;
            my += w * fy;
        }

        float v = ix[p] * mx + iy[p] * my;
        s_row_buf_buf[p] = v;
        float a = v < 0.0f ? -v : v;
        if (a > maxabs) maxabs = a;
    }

    if (maxabs > 0.0f) {
        float inv = 1.0f / maxabs;
        for (uint32_t p = 0; p < s_n_pixels; p++) {
            s_row_buf_buf[p] *= inv;
        }
    }

    (void)inj;
}

int calibration_begin_from_dataset(const char *dataset_filename, const char *output_filename)
{
    if (s_status == CALIB_STATUS_RUNNING) return 0;
    free_work_buffers();
    reset_state();

    s_last_res = FR_OK;
    s_stage = CALIB_STAGE_NONE;

    char dataset_path[64];
    char output_path[64];
    snprintf(dataset_path, sizeof(dataset_path), "0:/%s", dataset_filename);
    snprintf(output_path, sizeof(output_path), "0:/%s", output_filename);

    FIL ds;
    UINT br = 0;

    s_stage = CALIB_STAGE_OPEN_DATASET;
    s_last_res = f_open(&ds, dataset_path, FA_READ);
    if (s_last_res != FR_OK) {
        return fail(CALIB_STAGE_OPEN_DATASET, s_last_res);
    }

    BinFileHeader hdr;
    s_stage = CALIB_STAGE_READ_HEADER;
    s_last_res = f_read(&ds, &hdr, sizeof(hdr), &br);
    if (s_last_res != FR_OK || br != sizeof(hdr)) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_HEADER, s_last_res != FR_OK ? s_last_res : FR_DISK_ERR);
        return 0;
    }

    if (hdr.magic != DATASET_MAGIC_EITB) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_HEADER, FR_INVALID_OBJECT);
        return 0;
    }

    s_n_meas = hdr.n_meas;
    s_n_inj = hdr.n_inj;
    s_image_size = CAL_IMAGE_SIZE;
    s_n_elec = hdr.curr_pattern_rows;

    if (hdr.curr_pattern_rows == 0 || hdr.meas_pattern_rows == 0) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_PATTERNS, FR_INVALID_OBJECT);
        return 0;
    }

    if (hdr.curr_pattern_rows != hdr.meas_pattern_rows) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_PATTERNS, FR_INVALID_OBJECT);
        return 0;
    }

    if (s_n_meas == 0 || s_n_inj == 0) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_HEADER, FR_INVALID_OBJECT);
        return 0;
    }

    s_n_pixels = (uint32_t)s_image_size * (uint32_t)s_image_size;
    if (s_n_pixels > CAL_MAX_PIXELS) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_HEADER, FR_INVALID_PARAMETER);
        return 0;
    }
    s_n_rows_total = (uint32_t)s_n_inj * (uint32_t)s_n_meas;

    /* Skip Uel float array */
    FSIZE_t skip = (FSIZE_t)((uint32_t)s_n_meas * (uint32_t)s_n_inj * (uint32_t)sizeof(float));
    s_last_res = f_lseek(&ds, (FSIZE_t)sizeof(hdr) + skip);
    if (s_last_res != FR_OK) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_HEADER, s_last_res);
        return 0;
    }

    /* Read CurrentPattern int8: [n_elec * n_inj] */
    uint32_t cp_size = (uint32_t)hdr.curr_pattern_rows * (uint32_t)s_n_inj;
    uint32_t mp_size = (uint32_t)hdr.meas_pattern_rows * (uint32_t)s_n_meas;

    s_cp = (int8_t *)malloc(cp_size);
    s_mp = (int8_t *)malloc(mp_size);
    if (!s_cp || !s_mp) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_PATTERNS, FR_NOT_ENOUGH_CORE);
        return 0;
    }

    s_stage = CALIB_STAGE_READ_PATTERNS;
    s_last_res = f_read(&ds, s_cp, cp_size, &br);
    if (s_last_res != FR_OK || br != cp_size) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_PATTERNS, s_last_res != FR_OK ? s_last_res : FR_DISK_ERR);
        return 0;
    }

    s_last_res = f_read(&ds, s_mp, mp_size, &br);
    if (s_last_res != FR_OK || br != mp_size) {
        f_close(&ds);
        fail(CALIB_STAGE_READ_PATTERNS, s_last_res != FR_OK ? s_last_res : FR_DISK_ERR);
        return 0;
    }

    f_close(&ds);

    /* Use SDRAM for big electrode field precompute buffer; avoid heap exhaustion. */
    s_elec_field = (float *)CAL_ELEC_FIELD_ADDR;

    s_stage = CALIB_STAGE_PRECOMPUTE_FIELDS;
    precompute_electrode_fields(1.1f, 0.4f);

    /* Open output and write header */
    s_stage = CALIB_STAGE_OPEN_OUTPUT;
    s_last_res = f_open(&s_out, output_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (s_last_res != FR_OK) {
        fail(CALIB_STAGE_OPEN_OUTPUT, s_last_res);
        return 0;
    }

    SensitivityMatrixHeader sh;
    memset(&sh, 0, sizeof(sh));
    sh.magic = SENS_MAGIC;
    sh.n_measurements = s_n_rows_total;
    sh.n_pixels = s_n_pixels;
    sh.image_size = s_image_size;
    sh.n_inj = s_n_inj;
    sh.n_meas = s_n_meas;

    UINT bw = 0;
    s_stage = CALIB_STAGE_WRITE_HEADER;
    s_last_res = f_write(&s_out, &sh, sizeof(sh), &bw);
    if (s_last_res != FR_OK || bw != sizeof(sh)) {
        fail(CALIB_STAGE_WRITE_HEADER, s_last_res != FR_OK ? s_last_res : FR_DISK_ERR);
        return 0;
    }

    s_inj_idx = 0;
    s_meas_idx = 0;
    build_inj_field((uint16_t)s_inj_idx);

    s_bytes_written = sizeof(sh);
    s_bytes_total = sizeof(sh) + s_n_rows_total * s_n_pixels * (uint32_t)sizeof(float);

    s_status = CALIB_STATUS_RUNNING;
    s_stage = CALIB_STAGE_WRITE_DATA;
    return 1;
}

calib_status_t calibration_step(calib_progress_t *out)
{
    if (out) {
        out->status = s_status;
        out->stage = s_stage;
        out->bytes_written = s_bytes_written;
        out->bytes_total = s_bytes_total;
        out->n_inj = s_n_inj;
        out->n_meas = s_n_meas;
        out->image_size = s_image_size;
        out->fresult = s_last_res;
    }

    if (s_status != CALIB_STATUS_RUNNING) {
        return s_status;
    }

    if (s_inj_idx >= s_n_inj) {
        s_stage = CALIB_STAGE_CLOSE_FILE;
        s_last_res = f_close(&s_out);
        if (s_last_res != FR_OK) {
            return fail(CALIB_STAGE_CLOSE_FILE, s_last_res);
        }
        s_status = CALIB_STATUS_DONE;
        if (out) out->status = s_status;
        free_work_buffers();
        return s_status;
    }

    /* Compute + write one row per call */
    compute_row((uint16_t)s_inj_idx, (uint16_t)s_meas_idx);

    UINT bw = 0;
    s_last_res = f_write(&s_out, s_row_buf_buf, (UINT)(s_n_pixels * sizeof(float)), &bw);
    if (s_last_res != FR_OK || bw != (UINT)(s_n_pixels * sizeof(float))) {
        return fail(CALIB_STAGE_WRITE_DATA, s_last_res != FR_OK ? s_last_res : FR_DISK_ERR);
    }

    s_bytes_written += bw;

    s_meas_idx++;
    if (s_meas_idx >= s_n_meas) {
        s_meas_idx = 0;
        s_inj_idx++;
        if (s_inj_idx < s_n_inj) {
            build_inj_field((uint16_t)s_inj_idx);
        }
    }

    if (out) {
        out->bytes_written = s_bytes_written;
        out->fresult = s_last_res;
    }

    return s_status;
}

void calibration_cancel(void)
{
    if (s_status == CALIB_STATUS_RUNNING) {
        f_close(&s_out);
    }
    free_work_buffers();
    reset_state();
}

FRESULT calibration_last_fresult(void)
{
    return s_last_res;
}

calib_stage_t calibration_last_stage(void)
{
    return s_stage;
}

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALIB_STATUS_IDLE = 0,
    CALIB_STATUS_RUNNING,
    CALIB_STATUS_DONE,
    CALIB_STATUS_ERROR,
} calib_status_t;

typedef enum {
    CALIB_STAGE_NONE = 0,
    CALIB_STAGE_OPEN_DATASET,
    CALIB_STAGE_READ_HEADER,
    CALIB_STAGE_READ_PATTERNS,
    CALIB_STAGE_PRECOMPUTE_FIELDS,
    CALIB_STAGE_OPEN_OUTPUT,
    CALIB_STAGE_WRITE_HEADER,
    CALIB_STAGE_WRITE_DATA,
    CALIB_STAGE_CLOSE_FILE,
} calib_stage_t;

typedef struct {
    calib_status_t status;
    calib_stage_t stage;
    uint32_t bytes_written;
    uint32_t bytes_total;
    uint16_t n_inj;
    uint16_t n_meas;
    uint16_t image_size;
    FRESULT fresult;
} calib_progress_t;

/* Computes (generates) the sensitivity matrix using patterns from dataset_filename
 * (e.g. "datamat_1_0.bin") and writes output_filename (e.g. "sensitivity_matrix.bin")
 * at the SD root.
 * Returns 1 on success (started), 0 on failure.
 */
int sensitivity_matrix_begin_from_dataset(const char *dataset_filename, const char *output_filename);

/* Performs a small amount of work and returns current status.
 * Call periodically from an LVGL timer.
 */
calib_status_t sensitivity_matrix_step(calib_progress_t *out);

void sensitivity_matrix_cancel(void);

FRESULT sensitivity_matrix_last_fresult(void);
calib_stage_t sensitivity_matrix_last_stage(void);

/* Backward-compatible API (older code used "calibration" wording). */
int calibration_begin_from_dataset(const char *dataset_filename, const char *output_filename);
calib_status_t calibration_step(calib_progress_t *out);
void calibration_cancel(void);
FRESULT calibration_last_fresult(void);
calib_stage_t calibration_last_stage(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H */

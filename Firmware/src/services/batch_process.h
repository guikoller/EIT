/**
 * Batch Processing Service
 *
 * Runs LBP reconstruction on every dataset found on the SD card
 * and saves both a BMP image and a CSV of the raw float matrix
 * into "0:/batch/" on the SD card.
 *
 * Usage (from an LVGL timer):
 *   1. Call batch_process_begin() once to start.
 *   2. Call batch_process_step() repeatedly (~25 ms timer).
 *      Each call processes one dataset file.
 *   3. When step() returns BATCH_DONE or BATCH_ERROR, stop the timer.
 */
#ifndef BATCH_PROCESS_H
#define BATCH_PROCESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BATCH_IDLE,
    BATCH_RUNNING,
    BATCH_DONE,
    BATCH_ERROR
} batch_status_t;

typedef struct {
    batch_status_t status;
    uint16_t       current;          /**< 1-based index of file just processed */
    uint16_t       total;            /**< Total target files to process        */
    char           current_file[64]; /**< Filename being / just processed      */
    char           error_msg[64];    /**< Error description (on BATCH_ERROR)   */
} batch_progress_t;

/**
 * Scan SD for datamat_*.bin files, create "0:/batch/" folder,
 * and prepare internal state for stepping.
 *
 * @return 1 on success, 0 on failure (call batch_process_error() for detail).
 */
int batch_process_begin(void);

/**
 * After batch_process_begin() returns 0, returns a human-readable
 * error string describing what went wrong.
 */
const char *batch_process_error(void);

/**
 * Process the next dataset file (one per call).
 *
 * Loads dataset → runs LBP → saves BMP + CSV → frees memory.
 *
 * @param[out] progress  Filled with current status and progress info.
 * @return Current batch_status_t.
 */
batch_status_t batch_process_step(batch_progress_t *progress);

#ifdef __cplusplus
}
#endif

#endif /* BATCH_PROCESS_H */

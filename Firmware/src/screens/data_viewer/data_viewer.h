#ifndef DATA_VIEWER_H
#define DATA_VIEWER_H

#include "lvgl.h"
#include <stdint.h>

/**
 * Create data viewer screen for displaying binary file contents
 * @param filename Name of the .bin file to load from SD card
 */
void data_viewer_create(const char *filename);

/**
 * Destroy data viewer screen and return to file browser
 */
void data_viewer_destroy(void);

/**
 * Get loaded Uel data for reconstruction
 * @param out_n_meas Pointer to store number of measurements
 * @param out_n_inj Pointer to store number of injections
 * @return 2D array [n_meas][n_inj] or NULL if not loaded
 */
float** data_viewer_get_uel(uint16_t *out_n_meas, uint16_t *out_n_inj);

#endif // DATA_VIEWER_H

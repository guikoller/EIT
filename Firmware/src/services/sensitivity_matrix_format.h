/**
 * Sensitivity Matrix binary file format header.
 *
 * Shared between:
 *   - calibration service (writer)
 *   - LBP reconstruction  (reader)
 */
#ifndef SENSITIVITY_MATRIX_FORMAT_H
#define SENSITIVITY_MATRIX_FORMAT_H

#include <stdint.h>

#define SENS_MATRIX_MAGIC 0x53454E53u  /* 'SENS' */

typedef struct {
    uint32_t magic;           /* SENS_MATRIX_MAGIC */
    uint32_t n_measurements;  /* Total measurements (n_inj * n_meas) */
    uint32_t n_pixels;        /* Pixels in image (image_size^2) */
    uint32_t image_size;      /* Image side length (e.g. 32) */
    uint32_t n_inj;           /* Number of injection patterns */
    uint32_t n_meas;          /* Number of measurements per injection */
    uint32_t reserved[2];     /* Reserved for future use */
} __attribute__((packed)) SensitivityMatrixHeader;

_Static_assert(sizeof(SensitivityMatrixHeader) == 32, "SensitivityMatrixHeader must be 32 bytes");

#endif /* SENSITIVITY_MATRIX_FORMAT_H */

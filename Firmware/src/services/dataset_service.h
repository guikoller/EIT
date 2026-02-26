#ifndef DATASET_SERVICE_H
#define DATASET_SERVICE_H

#include "dataset_format.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BinFileHeader header;
    float *uel_1d;
    int8_t *curr_pattern;
    int8_t *meas_pattern;
} dataset_t;

/* Loads full dataset: header + Uel + optional CurrentPattern + optional MeasPattern.
 * Returns 0 on success, <0 on error.
 */
int dataset_service_load_full(const char *filename, dataset_t *out);

void dataset_service_free(dataset_t *ds);

/* Loads Uel only, returning a 2D view (pointer array) with [0] pointing to the 1D buffer.
 * The returned structure must be freed with dataset_service_free_uel_2d().
 */
float **dataset_service_load_uel_2d(const char *filename, uint16_t *out_n_meas, uint16_t *out_n_inj);

void dataset_service_free_uel_2d(float **uel_2d);

#ifdef __cplusplus
}
#endif

#endif /* DATASET_SERVICE_H */

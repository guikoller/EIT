#include "dataset_service.h"

#include "ff.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int open_dataset(const char *filename, FIL *out_file)
{
    if (!filename || !out_file) return -1;

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "0:/%s", filename);

    FRESULT res = f_open(out_file, filepath, FA_READ);
    return (res == FR_OK) ? 0 : -1;
}

static int read_exact(FIL *file, void *buf, UINT size)
{
    if (!file || !buf) return -1;
    UINT br = 0;
    FRESULT res = f_read(file, buf, size, &br);
    if (res != FR_OK) return -1;
    if (br != size) return -1;
    return 0;
}

int dataset_service_load_full(const char *filename, dataset_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    FIL file;
    if (open_dataset(filename, &file) != 0) {
        return -1;
    }

    if (read_exact(&file, &out->header, (UINT)sizeof(out->header)) != 0) {
        f_close(&file);
        return -2;
    }

    if (out->header.magic != DATASET_MAGIC_EITB) {
        f_close(&file);
        return -3;
    }

    uint32_t n_meas = out->header.n_meas;
    uint32_t n_inj  = out->header.n_inj;

    uint32_t uel_count = n_meas * n_inj;
    if (uel_count == 0u) {
        f_close(&file);
        return -4;
    }

    out->uel_1d = (float *)malloc(uel_count * sizeof(float));
    if (!out->uel_1d) {
        f_close(&file);
        return -5;
    }

    if (read_exact(&file, out->uel_1d, (UINT)(uel_count * sizeof(float))) != 0) {
        dataset_service_free(out);
        f_close(&file);
        return -6;
    }

    if (out->header.curr_pattern_rows > 0u) {
        uint32_t curr_sz = (uint32_t)out->header.curr_pattern_rows * n_inj;
        out->curr_pattern = (int8_t *)malloc(curr_sz);
        if (!out->curr_pattern) {
            dataset_service_free(out);
            f_close(&file);
            return -7;
        }
        if (read_exact(&file, out->curr_pattern, (UINT)curr_sz) != 0) {
            dataset_service_free(out);
            f_close(&file);
            return -8;
        }
    }

    if (out->header.meas_pattern_rows > 0u) {
        uint32_t meas_sz = (uint32_t)out->header.meas_pattern_rows * n_meas;
        out->meas_pattern = (int8_t *)malloc(meas_sz);
        if (!out->meas_pattern) {
            dataset_service_free(out);
            f_close(&file);
            return -9;
        }
        if (read_exact(&file, out->meas_pattern, (UINT)meas_sz) != 0) {
            dataset_service_free(out);
            f_close(&file);
            return -10;
        }
    }

    f_close(&file);
    return 0;
}

void dataset_service_free(dataset_t *ds)
{
    if (!ds) return;

    if (ds->uel_1d) {
        free(ds->uel_1d);
        ds->uel_1d = NULL;
    }
    if (ds->curr_pattern) {
        free(ds->curr_pattern);
        ds->curr_pattern = NULL;
    }
    if (ds->meas_pattern) {
        free(ds->meas_pattern);
        ds->meas_pattern = NULL;
    }
    memset(&ds->header, 0, sizeof(ds->header));
}

float **dataset_service_load_uel_2d(const char *filename, uint16_t *out_n_meas, uint16_t *out_n_inj)
{
    if (out_n_meas) *out_n_meas = 0;
    if (out_n_inj) *out_n_inj = 0;

    dataset_t ds;
    memset(&ds, 0, sizeof(ds));

    FIL file;
    if (open_dataset(filename, &file) != 0) {
        return NULL;
    }

    if (read_exact(&file, &ds.header, (UINT)sizeof(ds.header)) != 0) {
        f_close(&file);
        return NULL;
    }

    if (ds.header.magic != DATASET_MAGIC_EITB) {
        f_close(&file);
        return NULL;
    }

    uint32_t n_meas = ds.header.n_meas;
    uint32_t n_inj  = ds.header.n_inj;

    uint32_t uel_count = n_meas * n_inj;
    if (uel_count == 0u) {
        f_close(&file);
        return NULL;
    }

    float *uel_1d = (float *)malloc(uel_count * sizeof(float));
    if (!uel_1d) {
        f_close(&file);
        return NULL;
    }

    if (read_exact(&file, uel_1d, (UINT)(uel_count * sizeof(float))) != 0) {
        free(uel_1d);
        f_close(&file);
        return NULL;
    }

    f_close(&file);

    float **uel_2d = (float **)malloc(n_meas * sizeof(float *));
    if (!uel_2d) {
        free(uel_1d);
        return NULL;
    }

    for (uint32_t i = 0; i < n_meas; i++) {
        uel_2d[i] = &uel_1d[i * n_inj];
    }

    if (out_n_meas) *out_n_meas = (uint16_t)n_meas;
    if (out_n_inj) *out_n_inj = (uint16_t)n_inj;

    return uel_2d;
}

void dataset_service_free_uel_2d(float **uel_2d)
{
    if (uel_2d) {
        if (uel_2d[0]) {
            free(uel_2d[0]);
        }
        free(uel_2d);
    }
}

#ifndef DATASET_FORMAT_H
#define DATASET_FORMAT_H

#include <stdint.h>

#define DATASET_MAGIC_EITB 0x45495442u /* 'EITB' */

/* Binary dataset file header (20 bytes)
 * Layout must match the Python generator and on-device readers.
 */
typedef struct {
    uint32_t magic;              /* DATASET_MAGIC_EITB */
    uint16_t n_meas;
    uint16_t n_inj;
    uint16_t image_size;
    uint16_t curr_pattern_rows;
    uint16_t meas_pattern_rows;
    uint16_t reserved;
    uint32_t reserved2;
} __attribute__((packed)) BinFileHeader;

_Static_assert(sizeof(BinFileHeader) == 20, "BinFileHeader must be 20 bytes");

#endif /* DATASET_FORMAT_H */

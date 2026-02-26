#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_FILENAME_MAX 64

typedef struct {
    char filename[STORAGE_FILENAME_MAX];
    uint32_t size;
    uint8_t is_valid;
} storage_file_entry_t;

/* Initializes SD card + mounts FatFs at "0:".
 * Returns 0 on success, -1 on failure.
 */
int storage_service_init(void);

/* Returns 1 if filesystem mounted, else 0. */
int storage_service_is_mounted(void);

/* Scans "0:/" for datasets.
 * Currently includes .bin and .csv like the legacy browser.
 * Returns 0 on success, -1 on failure.
 */
int storage_service_scan_root(storage_file_entry_t *entries, int max_entries, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_SERVICE_H */

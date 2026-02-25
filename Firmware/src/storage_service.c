#include "storage_service.h"

#include "ff.h"
#include "stm32f769i_discovery_sd.h"

#include <string.h>

static FATFS s_fs;
static uint8_t s_mounted = 0u;

DWORD get_fattime(void)
{
    /* Fixed timestamp: 2026-01-04 12:00:00 (used by FatFs for file times). */
    return ((2026 - 1980) << 25) | (1 << 21) | (4 << 16) | (12 << 11) | (0 << 5) | (0 >> 1);
}

static int ends_with_ci(const char *s, const char *suffix)
{
    if (!s || !suffix) return 0;
    size_t sl = strlen(s);
    size_t su = strlen(suffix);
    if (su == 0u || su > sl) return 0;

    const char *p = s + (sl - su);
    for (size_t i = 0; i < su; i++) {
        char a = p[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

int storage_service_init(void)
{
    uint8_t sd_state = BSP_SD_Init();
    if (sd_state != MSD_OK) {
        s_mounted = 0u;
        return -1;
    }

    FRESULT res = f_mount(&s_fs, "0:", 1);
    if (res == FR_OK) {
        s_mounted = 1u;
        return 0;
    }

    s_mounted = 0u;
    return -1;
}

int storage_service_is_mounted(void)
{
    return s_mounted ? 1 : 0;
}

int storage_service_scan_root(storage_file_entry_t *entries, int max_entries, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!entries || max_entries <= 0) return -1;
    if (!s_mounted) return -1;

    DIR dir;
    FILINFO fno;

    FRESULT res = f_opendir(&dir, "0:");
    if (res != FR_OK) {
        return -1;
    }

    int count = 0;
    while (count < max_entries) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        if (fno.fattrib & AM_DIR) {
            continue;
        }

        if (!ends_with_ci(fno.fname, ".bin")) {
            continue;
        }

        memset(&entries[count], 0, sizeof(entries[count]));
        strncpy(entries[count].filename, fno.fname, STORAGE_FILENAME_MAX - 1);
        entries[count].filename[STORAGE_FILENAME_MAX - 1] = '\0';
        entries[count].size = (uint32_t)fno.fsize;
        entries[count].is_valid = 1u;
        count++;
    }

    (void)f_closedir(&dir);

    if (out_count) *out_count = count;
    return 0;
}

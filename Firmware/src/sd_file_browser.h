#ifndef SD_FILE_BROWSER_H
#define SD_FILE_BROWSER_H

#include "lvgl/lvgl.h"

/**
 * Initialize and display SD card file browser
 */
void sd_file_browser_create(void);

/**
 * Initialize SD card
 * @return 0 on success, -1 on error
 */
int sd_card_init(void);

#endif /* SD_FILE_BROWSER_H */

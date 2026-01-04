#ifndef DATA_VIEWER_H
#define DATA_VIEWER_H

#include "lvgl.h"

/**
 * Create data viewer screen for displaying binary file contents
 * @param filename Name of the .bin file to load from SD card
 */
void data_viewer_create(const char *filename);

/**
 * Destroy data viewer screen and return to file browser
 */
void data_viewer_destroy(void);

#endif // DATA_VIEWER_H

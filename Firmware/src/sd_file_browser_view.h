#ifndef SD_FILE_BROWSER_VIEW_H
#define SD_FILE_BROWSER_VIEW_H

#include "lvgl/lvgl.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SD_BROWSER_FILENAME_MAX 64

typedef struct {
    char filename[SD_BROWSER_FILENAME_MAX];
    uint32_t size;
    uint8_t is_valid;
} sd_browser_file_entry_t;

typedef void (*sd_browser_on_load_cb_t)(void *ctx, const char *filename);
typedef void (*sd_browser_on_calibrate_cb_t)(void *ctx);

typedef struct {
    void *ctx;
    sd_browser_on_load_cb_t on_load;
    sd_browser_on_calibrate_cb_t on_calibrate;
} sd_file_browser_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *file_list;
    lv_obj_t *btn_load;
    lv_obj_t *btn_calibrate;
    lv_obj_t *label_title;
    lv_obj_t *label_status;

    lv_obj_t *selected_file_obj;
    char selected_filename[SD_BROWSER_FILENAME_MAX];

    sd_file_browser_view_bindings_t bindings;
} sd_file_browser_view_t;

void sd_file_browser_view_create(sd_file_browser_view_t *view, lv_obj_t *parent, const sd_file_browser_view_bindings_t *bindings);
void sd_file_browser_view_set_enabled(sd_file_browser_view_t *view, int enabled);
void sd_file_browser_view_set_status(sd_file_browser_view_t *view, const char *text);
void sd_file_browser_view_set_files(sd_file_browser_view_t *view, const sd_browser_file_entry_t *entries, int count);

#ifdef __cplusplus
}
#endif

#endif /* SD_FILE_BROWSER_VIEW_H */

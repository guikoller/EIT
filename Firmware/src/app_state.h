#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>

#define APP_FILENAME_MAX 64

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_BROWSER,
    APP_STATE_DATA_VIEWER,
    APP_STATE_RECON_VIEWER,
    APP_STATE_ERROR,
} app_state_id_t;

typedef enum {
    APP_SCREEN_NONE = 0,
    APP_SCREEN_BROWSER,
    APP_SCREEN_DATA_VIEWER,
    APP_SCREEN_RECON_VIEWER,
} app_screen_t;

typedef struct {
    app_state_id_t current_state;
    app_screen_t active_screen;
    app_screen_t requested_screen;

    uint8_t sd_ready;
    int last_error;

    char selected_file[APP_FILENAME_MAX];
} app_state_t;

#endif /* APP_STATE_H */

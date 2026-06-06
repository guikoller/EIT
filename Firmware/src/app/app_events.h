#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>

typedef enum {
    APP_EVENT_NONE = 0,
    APP_EVENT_BOOT_COMPLETE,
    APP_EVENT_OPEN_HOME,
    APP_EVENT_OPEN_SETTINGS,
    APP_EVENT_OPEN_WIFI_SETTINGS,
    APP_EVENT_OPEN_BROWSER,
    APP_EVENT_OPEN_DATA_VIEWER,
    APP_EVENT_OPEN_RECON_VIEWER,
    APP_EVENT_OPEN_SERIAL_MONITOR,
    APP_EVENT_BACK,
    APP_EVENT_ERROR,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    union {
        struct {
            uint8_t sd_ready;
        } boot;

        struct {
            int code;
        } error;

        struct {
            char filename[64];
        } nav;
    } data;
} app_event_t;

#endif /* APP_EVENTS_H */

#ifndef SERIAL_MONITOR_PRESENTER_H
#define SERIAL_MONITOR_PRESENTER_H

#include "serial_monitor_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    serial_monitor_view_t *view;
    uint8_t polling;
} serial_monitor_presenter_t;

void serial_monitor_presenter_init(serial_monitor_presenter_t *p,
                                   serial_monitor_view_t *v);
void serial_monitor_presenter_on_create(serial_monitor_presenter_t *p);
void serial_monitor_presenter_poll(serial_monitor_presenter_t *p);

void serial_monitor_presenter_on_back(void *ctx);
void serial_monitor_presenter_on_send(void *ctx, const char *cmd);
void serial_monitor_presenter_on_clear(void *ctx);
void serial_monitor_presenter_on_nav_home(void *ctx);
void serial_monitor_presenter_on_nav_eit(void *ctx);
void serial_monitor_presenter_on_nav_settings(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_MONITOR_PRESENTER_H */

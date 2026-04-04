#ifndef SERIAL_MONITOR_VIEW_H
#define SERIAL_MONITOR_VIEW_H

#include "lvgl/lvgl.h"
#include "screens/common/left_menu.h"

#define SERIAL_MONITOR_LOG_MAX_LEN 4096u

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*serial_monitor_nav_cb_t)(void *ctx);
typedef void (*serial_monitor_send_cb_t)(void *ctx, const char *cmd);

typedef struct {
    void *ctx;
    serial_monitor_nav_cb_t on_back;
    serial_monitor_nav_cb_t on_nav_home;
    serial_monitor_nav_cb_t on_nav_eit;
    serial_monitor_nav_cb_t on_nav_settings;
    serial_monitor_send_cb_t on_send;
    serial_monitor_nav_cb_t on_clear;
} serial_monitor_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *content;
    left_menu_t menu;
    lv_obj_t *label_title;

    lv_obj_t *log_panel;   /* scrollable container */
    lv_obj_t *label_log;   /* label inside log_panel */
    char      log_buf[SERIAL_MONITOR_LOG_MAX_LEN + 1u];
    uint32_t  log_len;

    lv_obj_t *ta_cmd;

    lv_obj_t *btn_send;
    lv_obj_t *btn_clear;
    lv_obj_t *btn_at;
    lv_obj_t *btn_at_rst;

    lv_obj_t *keyboard;

    serial_monitor_view_bindings_t bindings;
} serial_monitor_view_t;

void serial_monitor_view_create(serial_monitor_view_t *view,
                                lv_obj_t *parent,
                                const serial_monitor_view_bindings_t *bindings);

void serial_monitor_view_append_log(serial_monitor_view_t *view, const char *text);
void serial_monitor_view_clear_log(serial_monitor_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_MONITOR_VIEW_H */

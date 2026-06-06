#ifndef WIFI_SETTINGS_VIEW_H
#define WIFI_SETTINGS_VIEW_H

#include "lvgl/lvgl.h"
#include "screens/common/left_menu.h"
#include "services/wifi/wifi_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_settings_nav_cb_t)(void *ctx);
typedef void (*wifi_settings_save_cb_t)(void *ctx,
                                        const char *ssid,
                                        const char *password);
typedef void (*wifi_settings_connect_cb_t)(void *ctx,
                                           const char *ssid,
                                           const char *password);
typedef void (*wifi_settings_scan_cb_t)(void *ctx);

typedef struct {
    void *ctx;
    wifi_settings_nav_cb_t on_back;
    wifi_settings_nav_cb_t on_nav_home;
    wifi_settings_nav_cb_t on_nav_eit;
    wifi_settings_nav_cb_t on_nav_settings;
    wifi_settings_save_cb_t on_save;
    wifi_settings_connect_cb_t on_connect;
    wifi_settings_scan_cb_t on_scan;
} wifi_settings_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *content;
    left_menu_t menu;
    lv_obj_t *label_title;

    /* Status bar */
    lv_obj_t *status_bar;
    lv_obj_t *label_status_icon;
    lv_obj_t *label_status;

    /* Network section */
    lv_obj_t *dd_ssid;
    lv_obj_t *btn_scan;
    lv_obj_t *ta_password;

    /* Action buttons */
    lv_obj_t *btn_save;
    lv_obj_t *btn_connect;

    lv_obj_t *keyboard;

    wifi_settings_view_bindings_t bindings;
} wifi_settings_view_t;

void wifi_settings_view_create(wifi_settings_view_t *view,
                               lv_obj_t *parent,
                               const wifi_settings_view_bindings_t *bindings);

void wifi_settings_view_set_values(wifi_settings_view_t *view,
                                   const char *ssid,
                                   const char *password);

void wifi_settings_view_set_status(wifi_settings_view_t *view,
                                   const char *status,
                                   int is_ok);

void wifi_settings_view_set_actions_enabled(wifi_settings_view_t *view, int enabled);

void wifi_settings_view_populate_networks(wifi_settings_view_t *view,
                                          const wifi_scan_entry_t *entries,
                                          uint32_t count,
                                          const char *current_ssid);

void wifi_settings_view_set_scanning(wifi_settings_view_t *view, int scanning);

const char *wifi_settings_view_get_selected_ssid(wifi_settings_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETTINGS_VIEW_H */

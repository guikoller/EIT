#ifndef WIFI_SETTINGS_VIEW_H
#define WIFI_SETTINGS_VIEW_H

#include "lvgl/lvgl.h"
#include "screens/common/left_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_settings_back_cb_t)(void *ctx);
typedef void (*wifi_settings_save_cb_t)(void *ctx,
                                        const char *ssid,
                                        const char *password,
                                        const char *server);
typedef void (*wifi_settings_connect_cb_t)(void *ctx,
                                           const char *ssid,
                                           const char *password,
                                           const char *server);

typedef struct {
    void *ctx;
    wifi_settings_back_cb_t on_back;
    wifi_settings_back_cb_t on_nav_home;
    wifi_settings_back_cb_t on_nav_eit;
    wifi_settings_back_cb_t on_nav_settings;
    wifi_settings_save_cb_t on_save;
    wifi_settings_connect_cb_t on_connect;
} wifi_settings_view_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *content;
    left_menu_t menu;
    lv_obj_t *label_title;
    lv_obj_t *label_status;

    lv_obj_t *ta_ssid;
    lv_obj_t *ta_password;
    lv_obj_t *ta_server;

    lv_obj_t *btn_save;
    lv_obj_t *btn_connect;
    lv_obj_t *btn_back;

    lv_obj_t *keyboard;

    wifi_settings_view_bindings_t bindings;
} wifi_settings_view_t;

void wifi_settings_view_create(wifi_settings_view_t *view,
                               lv_obj_t *parent,
                               const wifi_settings_view_bindings_t *bindings);

void wifi_settings_view_set_values(wifi_settings_view_t *view,
                                   const char *ssid,
                                   const char *password,
                                   const char *server);

void wifi_settings_view_set_status(wifi_settings_view_t *view, const char *status);
void wifi_settings_view_set_actions_enabled(wifi_settings_view_t *view, int enabled);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETTINGS_VIEW_H */

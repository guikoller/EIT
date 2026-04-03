#ifndef WIFI_SETTINGS_PRESENTER_H
#define WIFI_SETTINGS_PRESENTER_H

#include "wifi_settings_view.h"
#include "services/wifi/wifi_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wifi_settings_view_t *view;
    wifi_service_config_t pending_cfg;
    uint8_t connect_pending;
} wifi_settings_presenter_t;

void wifi_settings_presenter_init(wifi_settings_presenter_t *p, wifi_settings_view_t *v);
void wifi_settings_presenter_on_create(wifi_settings_presenter_t *p);

void wifi_settings_presenter_on_back(void *ctx);
void wifi_settings_presenter_on_save(void *ctx,
                                     const char *ssid,
                                     const char *password,
                                     const char *server);
void wifi_settings_presenter_on_connect(void *ctx,
                                        const char *ssid,
                                        const char *password,
                                        const char *server);
void wifi_settings_presenter_on_nav_home(void *ctx);
void wifi_settings_presenter_on_nav_eit(void *ctx);
void wifi_settings_presenter_on_nav_settings(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SETTINGS_PRESENTER_H */

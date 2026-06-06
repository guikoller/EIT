#include "wifi_settings_presenter.h"

#include "app/app_coordinator.h"

#include <string.h>
#include <stdio.h>

static void cfg_from_inputs(wifi_service_config_t *cfg,
                            const char *ssid,
                            const char *password)
{
    if (!cfg) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    if (ssid) {
        strncpy(cfg->ssid, ssid, WIFI_SSID_MAX - 1u);
    }
    if (password) {
        strncpy(cfg->password, password, WIFI_PASSWORD_MAX - 1u);
    }
}

static void connect_async_cb(void *user_data)
{
    wifi_settings_presenter_t *p = (wifi_settings_presenter_t *)user_data;
    if (!p || !p->view) {
        return;
    }

    p->connect_pending = 0u;

    wifi_service_set_config(&p->pending_cfg);
    (void)wifi_service_save_config();

    if (wifi_service_connect() == 0) {
        wifi_settings_view_set_status(p->view, "Connected to WiFi", 1);
    } else {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s", wifi_service_last_error());
        wifi_settings_view_set_status(p->view, msg, -1);
    }

    wifi_settings_view_set_actions_enabled(p->view, 1);
}

static void scan_async_cb(void *user_data)
{
    wifi_settings_presenter_t *p = (wifi_settings_presenter_t *)user_data;
    if (!p || !p->view) {
        return;
    }

    wifi_scan_entry_t results[WIFI_SCAN_MAX];
    int count = wifi_service_scan(results, WIFI_SCAN_MAX);

    p->scan_pending = 0u;
    wifi_settings_view_set_scanning(p->view, 0);

    if (count < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Scan failed: %s", wifi_service_last_error());
        wifi_settings_view_set_status(p->view, msg, -1);
        return;
    }

    if (count == 0) {
        wifi_settings_view_set_status(p->view, "No networks found", 0);
        return;
    }

    /* Get the currently saved SSID so it's pre-selected */
    wifi_service_config_t cfg;
    wifi_service_get_config(&cfg);

    wifi_settings_view_populate_networks(p->view, results,
                                         (uint32_t)count, cfg.ssid);

    char msg[48];
    snprintf(msg, sizeof(msg), "Found %d network%s", count,
             count == 1 ? "" : "s");
    wifi_settings_view_set_status(p->view, msg, 1);
    wifi_settings_view_set_actions_enabled(p->view, 1);
}

void wifi_settings_presenter_init(wifi_settings_presenter_t *p, wifi_settings_view_t *v)
{
    if (!p) {
        return;
    }

    memset(p, 0, sizeof(*p));
    p->view = v;
}

void wifi_settings_presenter_on_create(wifi_settings_presenter_t *p)
{
    if (!p || !p->view) {
        return;
    }

    int init_rc = wifi_service_init();

    wifi_service_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    wifi_service_get_config(&cfg);

    wifi_settings_view_set_values(p->view, cfg.ssid, cfg.password);

    if (init_rc == 0) {
        wifi_settings_view_set_status(p->view, "ESP ready — tap Scan to find networks", 1);
    } else {
        char msg[96];
        snprintf(msg, sizeof(msg), "ESP offline: %s", wifi_service_last_error());
        wifi_settings_view_set_status(p->view, msg, -1);
    }
}

void wifi_settings_presenter_on_back(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_BACK;
    app_coordinator_post_event(&evt);
}

void wifi_settings_presenter_on_save(void *ctx,
                                     const char *ssid,
                                     const char *password)
{
    wifi_settings_presenter_t *p = (wifi_settings_presenter_t *)ctx;
    if (!p || !p->view) {
        return;
    }

    cfg_from_inputs(&p->pending_cfg, ssid, password);
    wifi_service_set_config(&p->pending_cfg);

    if (wifi_service_save_config() == 0) {
        wifi_settings_view_set_status(p->view, "Configuration saved", 1);
    } else {
        char msg[96];
        snprintf(msg, sizeof(msg), "Save error: %s", wifi_service_last_error());
        wifi_settings_view_set_status(p->view, msg, -1);
    }
}

void wifi_settings_presenter_on_connect(void *ctx,
                                        const char *ssid,
                                        const char *password)
{
    wifi_settings_presenter_t *p = (wifi_settings_presenter_t *)ctx;
    if (!p || !p->view) {
        return;
    }

    if (p->connect_pending) {
        return;
    }

    cfg_from_inputs(&p->pending_cfg, ssid, password);

    p->connect_pending = 1u;
    wifi_settings_view_set_actions_enabled(p->view, 0);
    wifi_settings_view_set_status(p->view, "Connecting...", 0);

    lv_async_call(connect_async_cb, p);
}

void wifi_settings_presenter_on_scan(void *ctx)
{
    wifi_settings_presenter_t *p = (wifi_settings_presenter_t *)ctx;
    if (!p || !p->view) {
        return;
    }

    if (p->scan_pending || p->connect_pending) {
        return;
    }

    p->scan_pending = 1u;
    wifi_settings_view_set_scanning(p->view, 1);
    wifi_settings_view_set_actions_enabled(p->view, 0);
    wifi_settings_view_set_status(p->view, "Scanning for networks...", 0);

    lv_async_call(scan_async_cb, p);
}

void wifi_settings_presenter_on_nav_home(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void wifi_settings_presenter_on_nav_eit(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

void wifi_settings_presenter_on_nav_settings(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}

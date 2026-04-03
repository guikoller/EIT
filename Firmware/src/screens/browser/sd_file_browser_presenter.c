#include "sd_file_browser_presenter.h"

#include "app/app_coordinator.h"
#include "services/storage_service.h"
#include "eit_config.h"

#include <string.h>

void sd_file_browser_presenter_init(sd_file_browser_presenter_t *presenter, sd_file_browser_view_t *view)
{
    if (!presenter) return;

    memset(presenter, 0, sizeof(*presenter));
    presenter->view = view;
}

void sd_file_browser_presenter_on_create(sd_file_browser_presenter_t *presenter)
{
    if (!presenter || !presenter->view) return;

    sd_file_browser_view_set_enabled(presenter->view, 1);

    storage_file_entry_t tmp[EIT_MAX_BROWSER_FILES];
    memset(tmp, 0, sizeof(tmp));

    sd_browser_file_entry_t entries[EIT_MAX_BROWSER_FILES];
    memset(entries, 0, sizeof(entries));

    int count = 0;
    if (storage_service_scan_root(tmp, EIT_MAX_BROWSER_FILES, &count) != 0) {
        count = 0;
    }

    for (int i = 0; i < count && i < (int)EIT_MAX_BROWSER_FILES; i++) {
        strncpy(entries[i].filename, tmp[i].filename, sizeof(entries[i].filename) - 1);
        entries[i].filename[sizeof(entries[i].filename) - 1] = '\0';
        entries[i].size = tmp[i].size;
        entries[i].is_valid = tmp[i].is_valid;
    }

    sd_file_browser_view_set_files(presenter->view, entries, count);
}

void sd_file_browser_presenter_on_back(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_BACK;
    app_coordinator_post_event(&evt);
}

void sd_file_browser_presenter_on_nav_home(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void sd_file_browser_presenter_on_nav_eit(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

void sd_file_browser_presenter_on_nav_settings(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}

void sd_file_browser_presenter_on_load(void *ctx, const char *filename)
{
    (void)ctx;
    if (!filename || filename[0] == '\0') return;

    const app_state_t *st = app_coordinator_get_state();

    app_event_t evt;
    if (st->settings.show_data_table) {
        evt.type = APP_EVENT_OPEN_DATA_VIEWER;
    } else {
        evt.type = APP_EVENT_OPEN_RECON_VIEWER;
    }
    strncpy(evt.data.nav.filename, filename, sizeof(evt.data.nav.filename) - 1);
    evt.data.nav.filename[sizeof(evt.data.nav.filename) - 1] = '\0';
    app_coordinator_post_event(&evt);
}

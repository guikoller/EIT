#include "home_presenter.h"

#include "app/app_coordinator.h"

#include <string.h>

void home_presenter_init(home_presenter_t *p, home_view_t *v)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->view = v;
}

void home_presenter_on_start(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

void home_presenter_on_settings(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}

void home_presenter_on_nav_home(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void home_presenter_on_nav_eit(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

void home_presenter_on_nav_settings(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}

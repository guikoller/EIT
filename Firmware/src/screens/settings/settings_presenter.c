#include "settings_presenter.h"

#include "app/app_coordinator.h"

#include <string.h>

void settings_presenter_init(settings_presenter_t *p, settings_view_t *v)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->view = v;
}

void settings_presenter_on_create(settings_presenter_t *p)
{
    if (!p || !p->view) return;

    /* Read current settings from the coordinator and push into the view */
    const app_state_t *st = app_coordinator_get_state();
    settings_view_set_values(p->view,
                             st->settings.algorithm,
                             st->settings.image_size,
                             st->settings.show_data_table);
}

void settings_presenter_on_back(void *ctx)
{
    (void)ctx;
    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void settings_presenter_on_algorithm(void *ctx, eit_algorithm_t algo)
{
    (void)ctx;
    /* Write directly — settings are shared via coordinator state */
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.algorithm = algo;
}

void settings_presenter_on_image_size(void *ctx, uint16_t size)
{
    (void)ctx;
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.image_size = size;
}

void settings_presenter_on_show_data_table(void *ctx, uint8_t value)
{
    (void)ctx;
    app_state_t *st = (app_state_t *)app_coordinator_get_state();
    st->settings.show_data_table = value;
}

#include "data_viewer.h"

#include "data_viewer_presenter.h"
#include "data_viewer_view.h"

#include <string.h>

static data_viewer_view_t s_view;
static data_viewer_presenter_t s_presenter;

void data_viewer_create(const char *filename)
{
    data_viewer_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    data_viewer_presenter_init(&s_presenter, &s_view);

    bindings.ctx = &s_presenter;
    bindings.on_return = data_viewer_presenter_on_return;
    bindings.on_run = data_viewer_presenter_on_run;
    bindings.on_tab_changed = data_viewer_presenter_on_tab_changed;
    bindings.on_nav_home = data_viewer_presenter_on_nav_home;
    bindings.on_nav_eit = data_viewer_presenter_on_nav_eit;
    bindings.on_nav_settings = data_viewer_presenter_on_nav_settings;

    data_viewer_view_create(&s_view, lv_screen_active(), &bindings);
    data_viewer_presenter_on_create(&s_presenter, filename);
}

void data_viewer_destroy(void)
{
    data_viewer_presenter_on_return(&s_presenter);
}

float **data_viewer_get_uel(uint16_t *out_n_meas, uint16_t *out_n_inj)
{
    return data_viewer_presenter_get_uel(&s_presenter, out_n_meas, out_n_inj);
}

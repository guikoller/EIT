#include "home_screen.h"

#include "home_presenter.h"
#include "home_view.h"

#include <string.h>

static home_view_t      s_view;
static home_presenter_t s_presenter;

void home_screen_create(void)
{
    home_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    home_presenter_init(&s_presenter, &s_view);

    bindings.ctx         = &s_presenter;
    bindings.on_start    = home_presenter_on_start;
    bindings.on_settings = home_presenter_on_settings;
    bindings.on_about    = home_presenter_on_about;

    home_view_create(&s_view, lv_screen_active(), &bindings);
}

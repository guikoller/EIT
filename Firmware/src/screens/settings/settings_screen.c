#include "settings_screen.h"

#include "settings_presenter.h"
#include "settings_view.h"

#include <string.h>

static settings_view_t      s_view;
static settings_presenter_t s_presenter;

void settings_screen_create(void)
{
    settings_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    settings_presenter_init(&s_presenter, &s_view);

    bindings.ctx              = &s_presenter;
    bindings.on_back          = settings_presenter_on_back;
    bindings.on_algorithm     = settings_presenter_on_algorithm;
    bindings.on_image_size    = settings_presenter_on_image_size;
    bindings.on_show_data_table = settings_presenter_on_show_data_table;
    bindings.on_calibrate       = settings_presenter_on_calibrate;

    settings_view_create(&s_view, lv_screen_active(), &bindings);
    settings_presenter_on_create(&s_presenter);
}

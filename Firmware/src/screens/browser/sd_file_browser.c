#include "sd_file_browser.h"

#include "sd_file_browser_presenter.h"
#include "sd_file_browser_view.h"

#include <string.h>

static sd_file_browser_view_t s_view;
static sd_file_browser_presenter_t s_presenter;

void sd_file_browser_create(void)
{
    sd_file_browser_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    sd_file_browser_presenter_init(&s_presenter, &s_view);

    bindings.ctx = &s_presenter;
    bindings.on_back = sd_file_browser_presenter_on_back;
    bindings.on_nav_home = sd_file_browser_presenter_on_nav_home;
    bindings.on_nav_eit = sd_file_browser_presenter_on_nav_eit;
    bindings.on_nav_settings = sd_file_browser_presenter_on_nav_settings;
    bindings.on_load = sd_file_browser_presenter_on_load;

    sd_file_browser_view_create(&s_view, lv_screen_active(), &bindings);
    sd_file_browser_presenter_on_create(&s_presenter);
}

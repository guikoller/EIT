#include "wifi_settings_screen.h"

#include "wifi_settings_presenter.h"
#include "wifi_settings_view.h"

#include <string.h>

static wifi_settings_view_t s_view;
static wifi_settings_presenter_t s_presenter;

void wifi_settings_screen_create(void)
{
    wifi_settings_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    wifi_settings_presenter_init(&s_presenter, &s_view);

    bindings.ctx = &s_presenter;
    bindings.on_back = wifi_settings_presenter_on_back;
    bindings.on_nav_home = wifi_settings_presenter_on_nav_home;
    bindings.on_nav_eit = wifi_settings_presenter_on_nav_eit;
    bindings.on_nav_settings = wifi_settings_presenter_on_nav_settings;
    bindings.on_save = wifi_settings_presenter_on_save;
    bindings.on_connect = wifi_settings_presenter_on_connect;

    wifi_settings_view_create(&s_view, lv_screen_active(), &bindings);
    wifi_settings_presenter_on_create(&s_presenter);
}

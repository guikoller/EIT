#include "serial_monitor_screen.h"

#include "serial_monitor_presenter.h"
#include "serial_monitor_view.h"

#include <string.h>

static serial_monitor_view_t s_view;
static serial_monitor_presenter_t s_presenter;

void serial_monitor_screen_create(void)
{
    serial_monitor_view_bindings_t bindings;
    memset(&bindings, 0, sizeof(bindings));

    serial_monitor_presenter_init(&s_presenter, &s_view);

    bindings.ctx = &s_presenter;
    bindings.on_back = serial_monitor_presenter_on_back;
    bindings.on_nav_home = serial_monitor_presenter_on_nav_home;
    bindings.on_nav_eit = serial_monitor_presenter_on_nav_eit;
    bindings.on_nav_settings = serial_monitor_presenter_on_nav_settings;
    bindings.on_send = serial_monitor_presenter_on_send;
    bindings.on_clear = serial_monitor_presenter_on_clear;

    serial_monitor_view_create(&s_view, lv_screen_active(), &bindings);
    serial_monitor_presenter_on_create(&s_presenter);
}

void serial_monitor_screen_poll(void)
{
    serial_monitor_presenter_poll(&s_presenter);
}
